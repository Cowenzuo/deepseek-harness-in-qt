#include "environmentchecker.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

#include "process/dependencyprobe.h"
#include "process/startwrapped.h"
#include "settings/appsettings.h"

namespace dshinqt {

EnvironmentChecker::EnvironmentChecker(QObject *parent)
    : QObject(parent)
{}

void EnvironmentChecker::autoDetect(AppSettings *settings) const
{
    // 只回填可执行文件路径（PATH 探测）；源码路径不含任何写死的默认值，
    // 探测不到就交给引导页由用户指定。
    if (settings->nodePath.isEmpty())
        settings->nodePath = findProgram(QStringLiteral("node"));

    if (settings->pnpmPath.isEmpty())
        settings->pnpmPath = findProgram(QStringLiteral("pnpm"));

    if (settings->gitPath.isEmpty())
        settings->gitPath = findProgram(QStringLiteral("git"));
}

bool EnvironmentChecker::isSourceValid(const QString &sourcePath)
{
    if (sourcePath.trimmed().isEmpty())
        return false;
    return QFileInfo::exists(QDir(sourcePath).filePath(QStringLiteral("pnpm-workspace.yaml")));
}

QString EnvironmentChecker::findProgram(const QString &name)
{
    return QStandardPaths::findExecutable(name);
}

QString EnvironmentChecker::programFor(const QString &explicitPath, const QString &fallback)
{
    return explicitPath.isEmpty() ? fallback : explicitPath;
}

QString EnvironmentChecker::runCapture(const QString &program, const QStringList &args, int timeoutMs) const
{
    QProcess p;
    startWrapped(&p, program, args);
    if (!p.waitForStarted(timeoutMs))
        return {};
    if (!p.waitForFinished(timeoutMs))
        return {};
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

namespace {
const QStringList kCheckOrder = {QStringLiteral("deepseek-harness 仓库路径"),
                                 QStringLiteral("dsh 依赖"),
                                 QStringLiteral("git"),
                                 QStringLiteral("Node.js"),
                                 QStringLiteral("pnpm")};
} // namespace

EnvItem EnvironmentChecker::checkOne(const QString &name, const AppSettings &settings) const
{
    EnvItem it;
    it.name = name;

    if (name == QStringLiteral("deepseek-harness 仓库路径")) {
        it.passed = isSourceValid(settings.sourcePath);
        it.detail =
            it.passed ? settings.sourcePath : QStringLiteral("未找到 pnpm-workspace.yaml：%1").arg(settings.sourcePath);
        return it;
    }

    if (name == QStringLiteral("dsh 依赖")) {
        if (!isSourceValid(settings.sourcePath)) {
            it.detail = QStringLiteral("源码路径无效，无法检查依赖");
            return it;
        }

        // 依赖/产物判定统一走 probeDependencies（与启动体检同一数据源）
        const QList<CheckItem> deps = probeDependencies(settings.sourcePath);
        QStringList missing;
        for (const auto &d : deps) {
            if (!d.passed)
                missing << d.detail;
        }
        it.passed = missing.isEmpty();
        it.detail = missing.isEmpty() ? QStringLiteral("依赖与构建产物齐全") : missing.join(QStringLiteral("、"));
        return it;
    }

    if (name == QStringLiteral("git")) {
        const QString ver =
            runCapture(programFor(settings.gitPath, QStringLiteral("git")), {QStringLiteral("--version")}, 3000);
        it.passed = !ver.isEmpty();
        it.detail = ver.isEmpty() ? QStringLiteral("未找到 git") : ver;
        return it;
    }

    if (name == QStringLiteral("Node.js")) {
        const QString ver =
            runCapture(programFor(settings.nodePath, QStringLiteral("node")), {QStringLiteral("-v")}, 3000);
        if (ver.isEmpty()) {
            it.detail = QStringLiteral("未找到 Node.js，请安装 Node.js ≥ 22.19 或指定路径");
            return it;
        }
        it.detail = ver;
        it.passed = nodeVersionAtLeast(ver, 22, 19);
        return it;
    }

    if (name == QStringLiteral("pnpm")) {
        const QString ver =
            runCapture(programFor(settings.pnpmPath, QStringLiteral("pnpm")), {QStringLiteral("-v")}, 3000);
        it.passed = !ver.isEmpty();
        it.detail = ver.isEmpty() ? QStringLiteral("未找到 pnpm") : ver;
        return it;
    }

    return it;
}

void EnvironmentChecker::checkAsync(const AppSettings &settings)
{
    if (m_asyncRunning)
        return; // 旧校验链仍在推进，忽略重入请求
    m_asyncRunning = true;
    m_asyncSettings = settings;
    m_asyncNames = kCheckOrder;
    m_asyncIndex = 0;
    runNext();
}

void EnvironmentChecker::runNext()
{
    if (m_asyncIndex >= m_asyncNames.size()) {
        m_asyncRunning = false;
        emit checkCompleted();
        return;
    }

    const QString name = m_asyncNames[m_asyncIndex];
    const int index = m_asyncIndex;
    emit checkStarted(index, name);

    // 单项同步校验；项与项之间通过事件循环刷新 UI
    const EnvItem item = checkOne(name, m_asyncSettings);
    emit itemChecked(index, item);

    m_asyncIndex++;
    QTimer::singleShot(0, this, &EnvironmentChecker::runNext);
}

} // namespace dshinqt
