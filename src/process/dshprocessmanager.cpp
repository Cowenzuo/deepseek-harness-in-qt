#include "dshprocessmanager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include "readywaiter.h"
#include "settings/appsettings.h"
#include "startwrapped.h"

namespace dshinqt {

namespace {
// 反查兜底候选源码根：仅本机常见位置，可由配置/命令行解析优先命中覆盖
const char *kFallbackSourceRoot = "D:/framework/deepseek-harness";
// dsh web 启动打印行：dsh web: http://127.0.0.1:3081/?token=<base64url>
const char *kWebTokenPattern = "\\?token=([A-Za-z0-9_-]+)";
} // namespace

QString DshProcessManager::extractWebToken(const QString &logText)
{
    const QRegularExpression re(QString::fromLatin1(kWebTokenPattern));
    const QRegularExpressionMatch m = re.match(logText);
    return m.hasMatch() ? m.captured(1) : QString();
}

QString DshProcessManager::stateName(State s)
{
    switch (s) {
    case State::Idle: return QStringLiteral("Idle");
    case State::Starting: return QStringLiteral("Starting");
    case State::Running: return QStringLiteral("Running");
    case State::Stopping: return QStringLiteral("Stopping");
    case State::Crashed: return QStringLiteral("Crashed");
    }
    return QStringLiteral("?");
}

DshProcessManager::DshProcessManager(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    m_waiter = new ReadyWaiter(this);
    connect(m_waiter, &ReadyWaiter::ready, this, &DshProcessManager::onWaiterReady);
    connect(m_waiter, &ReadyWaiter::timeout, this, &DshProcessManager::onWaiterTimeout);

    m_pollTimer.setInterval(1000);
    connect(&m_pollTimer, &QTimer::timeout, this, &DshProcessManager::onPollTick);

    const QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/config");
    QDir().mkpath(dir);
    m_logPath = QDir(dir).filePath(QStringLiteral("dsh-web.log"));
    m_sourceFile = QDir(dir).filePath(QStringLiteral("service-source.txt"));
}

// 常驻服务：析构不杀服务（dsh 已脱离外壳进程树）。
DshProcessManager::~DshProcessManager() = default;

void DshProcessManager::attach()
{
    qDebug() << "[SVC] attach() 服务已在运行，直接连接";
    emit logOutput(QStringLiteral("检测到 deepseek-harness 已在运行，直接连接"), false);
    // 从日志提取认证 token（dsh web 打印的 ?token=）；随后从文件尾开始 tail
    QFile f(m_logPath);
    if (f.open(QIODevice::ReadOnly)) {
        const QString token = extractWebToken(QString::fromUtf8(f.readAll()));
        if (!token.isEmpty()) {
            m_webToken = token;
            m_waiter->setToken(token);
            qDebug() << "[SVC] attach 已提取认证 token";
        }
        m_logPos = f.size();
        f.close();
    }
    m_pollTimer.start();
    setState(State::Running);
}

QString DshProcessManager::webUrl() const
{
    QString url = m_settings->webUrl();
    if (!m_webToken.isEmpty()) {
        QUrl u(url);
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("token"), m_webToken);
        u.setQuery(q);
        url = u.toString();
    }
    return url;
}

void DshProcessManager::start()
{
    if (m_state == State::Starting || m_state == State::Running)
        return;

    qDebug() << "[SVC] start() 异步清理后启动";
    // 异步清理可能残留的旧实例，避免端口冲突 / 连到旧服务
    const qint64 gen = ++m_opGeneration;
    killByPort([this, gen]() {
        if (gen != m_opGeneration)
            return; // 过期操作（已被新的 start/stop/restart 取代）
        beginLaunch();
    }, gen);
}

void DshProcessManager::beginLaunch()
{
    qDebug() << "[SVC] beginLaunch()";
    // 直接用 node 启动 dsh（真正的后台进程，绕过 pnpm/cmd.exe 命令行中间层）。
    // dsh 脚本固定为：node --import tsx/esm apps/cli/src/bin.ts <subcommand>
    const QString node = m_settings->nodePath.isEmpty() ? QStringLiteral("node") : m_settings->nodePath;

    QStringList args;
    args << QStringLiteral("--import") << QStringLiteral("tsx/esm") << QStringLiteral("apps/cli/src/bin.ts")
         << QStringLiteral("web") << QStringLiteral("--host") << QStringLiteral("127.0.0.1") << QStringLiteral("--port")
         << QString::number(m_settings->webPort) << QStringLiteral("--no-open"); // 壳内嵌 UI 承载，不弹默认浏览器

    QFile f(m_logPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.close();
    m_logPos = 0;
    setState(State::Starting);

    qDebug() << "[SVC] beginLaunch node=" << node << "port=" << m_settings->webPort;
    const bool ok = startDetachedWrapped(node, args, m_settings->sourcePath, m_logPath);
    qDebug() << "[SVC] startDetachedWrapped ok=" << ok;
    if (!ok) {
        emit logOutput(QStringLiteral("分离启动失败"), true);
        setState(State::Crashed);
        return;
    }

    emit logOutput(QStringLiteral("已后台启动：%1").arg(node), false);
    writeServiceSource();
    m_waiter->wait(QStringLiteral("127.0.0.1"), m_settings->webPort, 60000);
    m_pollTimer.start();
}

void DshProcessManager::stop()
{
    qDebug() << "[SVC] stop()";
    m_waiter->stop();
    m_pollTimer.stop();

    const qint64 gen = ++m_opGeneration;
    if (m_state == State::Running || m_state == State::Starting) {
        setState(State::Stopping);
        emit logOutput(QStringLiteral("停止服务..."), false);
    }

    // 异步杀端口进程，完成后置 Idle
    killByPort([this, gen]() {
        if (gen != m_opGeneration)
            return; // 迟到的 stop 回调不得打回 Idle / 删除新写入的记录
        clearServiceSource();
        setState(State::Idle);
        emit logOutput(QStringLiteral("服务已停止"), false);
    }, gen);
}

void DshProcessManager::restart()
{
    qDebug() << "[SVC] restart() 杀旧启新";
    m_waiter->stop();
    m_pollTimer.stop();
    // 异步杀旧，完成后直接分离启动（不经过 Idle 中间态）
    const qint64 gen = ++m_opGeneration;
    killByPort([this, gen]() {
        if (gen != m_opGeneration)
            return;
        beginLaunch();
    }, gen);
}

void DshProcessManager::ensureRunning()
{
    qDebug() << "[SVC] ensureRunning() 异步探测端口";
    // 单次异步探测端口：全程不阻塞主线程
    m_waiter->probeOnce(QStringLiteral("127.0.0.1"), m_settings->webPort, [this](bool up) {
        qDebug() << "[SVC] ensureRunning 探测结果 up=" << up;
        if (up) {
            if (isServiceMatching(m_settings->sourcePath)) {
                attach();
            } else {
                // 端口已有服务但与记录不一致：先尝试反查源码位置自动接管，
                // 反查失败（未知服务 / 路径不可用）才杀旧启新。
                inspectAsync([this](const ServiceInfo &info) {
                    if (info.ok && !info.sourceRoot.isEmpty()) {
                        qDebug() << "[SVC] 端口反查命中 sourceRoot=" << info.sourceRoot;
                        m_settings->sourcePath = info.sourceRoot;
                        m_settings->save();
                        writeServiceSource();
                        emit logOutput(QStringLiteral("已自动识别 dsh 源码：%1").arg(info.sourceRoot), false);
                        attach();
                    } else {
                        emit logOutput(QStringLiteral("检测到服务与当前仓库路径不一致，重启服务..."), false);
                        restart();
                    }
                });
            }
        } else {
            start();
        }
    });
}

bool DshProcessManager::isServiceMatching(const QString &sourcePath) const
{
    QFile f(m_sourceFile);
    if (!f.open(QIODevice::ReadOnly))
        return false; // 无记录，视为不匹配
    const QString recorded = QString::fromUtf8(f.readAll()).trimmed();
    return recorded == sourcePath;
}

void DshProcessManager::writeServiceSource()
{
    QFile f(m_sourceFile);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(m_settings->sourcePath.toUtf8());
        f.close();
    }
}

void DshProcessManager::clearServiceSource()
{
    QFile::remove(m_sourceFile);
}

bool DshProcessManager::isRunning() const
{
    return m_state == State::Running || m_state == State::Starting;
}

void DshProcessManager::setState(State s)
{
    if (m_state == s)
        return;
    qDebug() << "[SVC] state:" << stateName(m_state) << "->" << stateName(s);
    m_state = s;
    emit stateChanged(s);
}

void DshProcessManager::killByPort(std::function<void()> onDone, qint64 generation)
{
    qDebug() << "[SVC] killByPort() 开始 gen=" << generation;
#ifdef Q_OS_WIN
    // 异步 netstat 定位占用 webPort 的 PID（不阻塞主线程）
    auto *netstat = new QProcess(this);
    connect(netstat,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this, netstat, onDone, generation](int, QProcess::ExitStatus) {
                const QString out = QString::fromLocal8Bit(netstat->readAllStandardOutput());
                netstat->deleteLater();

                const QString portMarker = QLatin1Char(':') + QString::number(m_settings->webPort);
                QList<int> pids;
                for (const QString &line : out.split(QLatin1Char('\n'))) {
                    if (!line.contains(QStringLiteral("LISTENING")))
                        continue;
                    const QStringList fields = line.simplified().split(QLatin1Char(' '));
                    if (fields.size() < 5)
                        continue;
                    // 精确匹配本地地址字段（如 127.0.0.1:3080 / [::]:3080）的端口边界，
                    // 避免 ":3080" 子串误命中 :30800 等端口
                    if (!fields[1].endsWith(portMarker))
                        continue;
                    const int pid = fields.last().toInt();
                    if (pid > 0)
                        pids << pid;
                }

                qDebug() << "[SVC] killByPort 找到 pids=" << pids;
                if (pids.isEmpty()) {
                    qDebug() << "[SVC] killByPort 无残留，直接回调";
                    onDone();
                    return;
                }

                // 异步 taskkill /t /f 杀进程树，全部完成后回调
                auto *left = new int(pids.size());
                for (int pid : pids) {
                    auto *tk = new QProcess(this);
                    connect(tk,
                            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                            this,
                            [this, tk, left, onDone](int, QProcess::ExitStatus) {
                                tk->deleteLater();
                                if (--(*left) == 0) {
                                    delete left;
                                    onDone();
                                }
                            });
                    tk->start(
                        QStringLiteral("taskkill"),
                        {QStringLiteral("/pid"), QString::number(pid), QStringLiteral("/t"), QStringLiteral("/f")});
                }
            });
    netstat->start(QStringLiteral("netstat"), {QStringLiteral("-ano")});
#else
    Q_UNUSED(generation)
    onDone();
#endif
}

void DshProcessManager::onWaiterReady()
{
    qDebug() << "[SVC] onWaiterReady 端口就绪";
    // 端口就绪。boot 可能尚未稳定，轮询继续 tail 日志；
    // 稳定后由调用方延迟连接 webview。
    setState(State::Running);
    emit logOutput(QStringLiteral("Web UI 端口已就绪"), false);
}

void DshProcessManager::onWaiterTimeout()
{
    qDebug() << "[SVC] onWaiterTimeout 超时";
    m_waiter->stop(); // 停掉 ReadyWaiter 的轮询定时器，避免对死端口无限探测
    m_pollTimer.stop();
    emit logOutput(QStringLiteral("等待 Web UI 就绪超时"), true);
    setState(State::Crashed);
}

// 异步端口反查：netstat 定位 PID → PowerShell 取命令行/可执行路径 → 候选根探测源码位置
void DshProcessManager::inspectAsync(std::function<void(const ServiceInfo &)> cb)
{
#ifdef Q_OS_WIN
    const int port = m_settings->webPort;
    auto *netstat = new QProcess(this);
    connect(netstat,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this, netstat, port, cb](int, QProcess::ExitStatus) {
                const QString out = QString::fromLocal8Bit(netstat->readAllStandardOutput());
                netstat->deleteLater();

                int pid = 0;
                const QString portMarker = QLatin1Char(':') + QString::number(port);
                for (const QString &line : out.split(QLatin1Char('\n'))) {
                    if (!line.contains(QStringLiteral("LISTENING")))
                        continue;
                    const QStringList fields = line.simplified().split(QLatin1Char(' '));
                    if (fields.size() >= 5 && fields[1].endsWith(portMarker)) {
                        pid = fields.last().toInt();
                        if (pid > 0)
                            break;
                    }
                }
                if (pid <= 0) {
                    cb(ServiceInfo());
                    return;
                }

                auto *ps = new QProcess(this);
                connect(ps,
                        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        this,
                        [this, ps, pid, cb](int, QProcess::ExitStatus) {
                            const QString out = QString::fromLocal8Bit(ps->readAllStandardOutput());
                            ps->deleteLater();
                            cb(parseServiceInfo(pid, out));
                        });
                const QString cmd = QStringLiteral("$p=Get-CimInstance Win32_Process -Filter 'ProcessId=%1'; "
                                                   "$p.CommandLine; '---'; $p.ExecutablePath")
                                        .arg(pid);
                ps->start(
                    QStringLiteral("powershell"),
                    {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"), QStringLiteral("-Command"), cmd});
            });
    netstat->start(QStringLiteral("netstat"), {QStringLiteral("-ano")});
#else
    cb(ServiceInfo());
#endif
}

ServiceInfo DshProcessManager::parseServiceInfo(qint64 pid, const QString &raw) const
{
    ServiceInfo info;
    info.pid = pid;
    QStringList lines;
    for (const QString &l : raw.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
        lines << l.trimmed();
    QString commandLine, exePath;
    const int sep = lines.indexOf(QStringLiteral("---"));
    if (sep > 0) {
        commandLine = lines.value(sep - 1);
        exePath = lines.value(sep + 1);
    }
    info.commandLine = commandLine;
    info.nodePath = exePath;

    // 端口上的进程必须确实是 dsh（命令行含 bin.ts）才算命中
    if (!commandLine.contains(QStringLiteral("bin.ts")))
        return info;
    info.ok = true;

    // 候选根探测：配置路径 → 命令行中的绝对 bin.ts 路径 → 常见位置
    QStringList candidates;
    if (!m_settings->sourcePath.isEmpty())
        candidates << m_settings->sourcePath;
    static const QRegularExpression absBin(QStringLiteral("([A-Za-z]:[^\"' ]*apps/cli/src/bin\\.ts)"));
    const QRegularExpressionMatch m = absBin.match(commandLine);
    if (m.hasMatch()) {
        QString p = QDir::fromNativeSeparators(m.captured(1));
        const int idx = p.indexOf(QStringLiteral("/apps/cli/src/bin.ts"));
        if (idx > 0)
            candidates << p.left(idx);
    }
    candidates << QString::fromLatin1(kFallbackSourceRoot);

    for (const QString &cand : candidates) {
        const QString norm = QDir::fromNativeSeparators(cand);
        if (QFileInfo::exists(norm + QStringLiteral("/apps/cli/src/bin.ts")) &&
            QFileInfo::exists(norm + QStringLiteral("/pnpm-workspace.yaml"))) {
            info.sourceRoot = norm;
            break;
        }
    }
    return info;
}

void DshProcessManager::onPollTick()
{
    readLogTail();
}

void DshProcessManager::readLogTail()
{
    QFile f(m_logPath);
    if (!f.open(QIODevice::ReadOnly))
        return;
    if (!f.seek(m_logPos)) {
        // 文件被外部截短/重建：从头重读，恢复 tail 监督
        m_logPos = 0;
        if (!f.seek(0)) {
            f.close();
            return;
        }
    }
    const QByteArray chunk = m_pendingLine + f.readAll(); // 与上次未闭合半行拼接
    m_logPos = f.pos();
    f.close();

    const QList<QByteArray> lines = chunk.split('\n');
    m_pendingLine = lines.last(); // 最后一段可能是不完整行（无结尾换行），留待下个 chunk
    for (int i = 0; i < lines.size() - 1; ++i) {
        const QString line = QString::fromUtf8(lines[i]).trimmed();
        if (line.isEmpty())
            continue;
        // 提取 dsh web 认证 token（启动打印行 dsh web: http://.../?token=xxx）
        const QString token = extractWebToken(line);
        if (!token.isEmpty() && token != m_webToken) {
            m_webToken = token;
            m_waiter->setToken(token);
            qDebug() << "[SVC] 已提取 Web UI 认证 token";
            emit logOutput(QStringLiteral("已获取 Web UI 认证 token"), false);
        }
        emit logOutput(line, false);
    }
    if (chunk.isEmpty())
        m_pendingLine.clear();
}

} // namespace dshinqt
