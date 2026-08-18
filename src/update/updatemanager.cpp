#include "updatemanager.h"

#include <QDate>
#include <QProcessEnvironment>

#include "git/gitclient.h"
#include "process/commandrunner.h"
#include "process/proxydetector.h"
#include "settings/appsettings.h"

namespace dshinqt {

namespace {
// 远程跟踪分支（remote/xxx）→ 本地分支名；无斜杠视为本地分支名原样返回
QString localBranchName(const QString &name)
{
    const int slash = name.indexOf(QLatin1Char('/'));
    return slash > 0 ? name.mid(slash + 1) : name;
}
} // namespace

UpdateManager::UpdateManager(AppSettings *settings, GitClient *git, DshProcessManager *proc, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_git(git)
    , m_proc(proc)
{
    m_runner = new CommandRunner(this);
    connect(m_runner, &CommandRunner::outputReady, this, [this](const QString &text) {
        emit logOutput(text, false);
    });
    connect(m_proc, &DshProcessManager::stateChanged, this, &UpdateManager::onProcStateChanged);

    m_startTimeout.setSingleShot(true);
    connect(&m_startTimeout, &QTimer::timeout, this, [this] { fail(QStringLiteral("dsh 启动超时")); });
}

void UpdateManager::start(const Target &target)
{
    if (m_stage != Stage::Idle)
        return;
    m_target = target;

    if (m_git->isDirty()) {
        fail(QStringLiteral("工作区存在未提交修改，已拒绝更新"));
        return;
    }

    if (m_proc->isRunning()) {
        setStage(Stage::Stopping);
        emit logOutput(QStringLiteral("停止 dsh..."), false);
        m_proc->stop(); // 异步按端口杀进程树，与后续 fetch 并行无害
    }

    setStage(Stage::Fetch);
    emit logOutput(QStringLiteral("> git fetch --all --prune"), false);
    runGitFetch();
}

void UpdateManager::setStage(Stage s)
{
    if (m_stage == s)
        return;
    m_stage = s;
    emit stageChanged(s);
}

// 通用 git 命令步骤：代理参数 + 输出转发 + 失败文案
void UpdateManager::runGitCommand(const QStringList &args, const QString &failPrefix,
                                  std::function<void()> onSuccess)
{
    m_runner->start(m_settings->gitProgram(), ProxyDetector::gitProxyArgs() + args, m_settings->sourcePath,
                    QProcessEnvironment(), [this, failPrefix, onSuccess](bool success, int code) {
                        if (m_stage == Stage::Idle)
                            return; // 已被 cancel 复位
                        if (!success) {
                            fail(QStringLiteral("%1（code=%2）").arg(failPrefix).arg(code));
                            return;
                        }
                        onSuccess();
                    });
}

void UpdateManager::runGitFetch()
{
    runGitCommand({QStringLiteral("fetch"), QStringLiteral("--all"), QStringLiteral("--prune")},
                  QStringLiteral("git fetch 失败"), [this] {
                      setStage(Stage::Checkout);
                      beginCheckout();
                  });
}

void UpdateManager::runGitPull()
{
    runGitCommand({QStringLiteral("pull"), QStringLiteral("--ff-only")}, QStringLiteral("git pull 失败"), [this] {
        setStage(Stage::Install);
        beginInstall();
    });
}

void UpdateManager::beginCheckout()
{
    if (m_target.kind == Target::Branch) {
        const QString local = localBranchName(m_target.value);
        runGitCheckoutStep({QStringLiteral("checkout"), local},
                           {QStringLiteral("checkout"), QStringLiteral("-b"), local, m_target.value},
                           QStringLiteral("切换分支失败"));
    } else {
        // 提交目标：自动建 dsh/yyyyMMdd-<hash7> 分支（避免 detached HEAD），同名分支已存在则直接检出
        const QString hash = m_target.value;
        const QString branchName =
            QStringLiteral("dsh/%1-%2").arg(QDate::currentDate().toString(QStringLiteral("yyyyMMdd")),
                                            hash.left(7));
        // 先在覆盖 m_target.value 之前构造 fallback（携带原始 hash）
        runGitCheckoutStep({QStringLiteral("checkout"), branchName},
                           {QStringLiteral("checkout"), QStringLiteral("-b"), branchName, hash},
                           QStringLiteral("切换提交失败"));
        m_target.value = branchName; // 后续日志展示用分支名
    }
}

// 异步 checkout 两步：先直接切换，失败再 -b 建分支切换；两步都失败才报错
void UpdateManager::runGitCheckoutStep(const QStringList &firstArgs, const QStringList &fallbackArgs,
                                       const QString &failPrefix)
{
    m_runner->start(m_settings->gitProgram(), ProxyDetector::gitProxyArgs() + firstArgs, m_settings->sourcePath,
                    QProcessEnvironment(), [this, fallbackArgs, failPrefix](bool success, int) {
                        if (m_stage == Stage::Idle)
                            return; // 已被 cancel 复位
                        if (success) {
                            afterCheckout();
                            return;
                        }
                        // 第一步失败（本地无此分支/需建分支）：走 fallback
                        runGitCommand(fallbackArgs, failPrefix, [this] { afterCheckout(); });
                    });
}

void UpdateManager::afterCheckout()
{
    emit logOutput(QStringLiteral("已切换到：%1").arg(m_target.value), false);

    if (m_target.kind == Target::Branch) {
        setStage(Stage::Pull);
        emit logOutput(QStringLiteral("> git pull --ff-only"), false);
        runGitPull();
    } else {
        setStage(Stage::Install);
        beginInstall();
    }
}

void UpdateManager::beginInstall()
{
    setStage(Stage::Install);
    emit logOutput(QStringLiteral("> pnpm install"), false);
    runPnpm({QStringLiteral("install")}, [this] { beginBuild(); });
}

void UpdateManager::beginBuild()
{
    setStage(Stage::Build);
    emit logOutput(QStringLiteral("> pnpm run build"), false);
    runPnpm({QStringLiteral("run"), QStringLiteral("build")}, [this] { beginStart(); });
}

void UpdateManager::beginStart()
{
    setStage(Stage::Starting);
    emit logOutput(QStringLiteral("重新启动 dsh..."), false);

    // 防护：服务已是 Running（如更新期间用户手动启动了服务）→ 直接完成，避免永久挂起
    if (m_proc->isRunning()) {
        done();
        return;
    }

    m_startTimeout.start(60000); // 兜底：60s 内未 Running 则按失败处理

    // 上次 stop 的 killByPort 尚未完成时，等 Idle 后再启动
    if (m_proc->state() == DshProcessManager::State::Stopping) {
        connect(m_proc, &DshProcessManager::stateChanged, this,
                [this](DshProcessManager::State s) {
                    if (s == DshProcessManager::State::Idle)
                        m_proc->start();
                },
                Qt::SingleShotConnection);
        return;
    }

    m_proc->start();
}

void UpdateManager::runPnpm(const QStringList &args, std::function<void()> onSuccess)
{
    const QString pnpm = m_settings->pnpmPath.isEmpty() ? QStringLiteral("pnpm") : m_settings->pnpmPath;
    m_runner->start(pnpm, args, m_settings->sourcePath, ProxyDetector::pnpmEnvironment(),
                    [this, onSuccess](bool success, int code) {
                        if (m_stage == Stage::Idle)
                            return; // 已被 cancel 复位
                        if (!success) {
                            fail(QStringLiteral("pnpm 命令失败（code=%1）").arg(code));
                            return;
                        }
                        onSuccess();
                    });
}

void UpdateManager::onProcStateChanged(DshProcessManager::State state)
{
    if (m_stage != Stage::Starting)
        return;
    if (state == DshProcessManager::State::Running) {
        done();
    } else if (state == DshProcessManager::State::Crashed) {
        fail(QStringLiteral("dsh 启动失败"));
    }
}

void UpdateManager::fail(const QString &error)
{
    if (m_stage == Stage::Idle)
        return; // 已被 cancel/finished 复位（防重入）
    m_startTimeout.stop();
    setStage(Stage::Failed);
    emit logOutput(error, true);
    setStage(Stage::Idle); // 先复位再通知结果，避免槽内同步发起新更新被 Idle 守卫拒绝
    emit finished(false, error);
}

void UpdateManager::done()
{
    if (m_stage == Stage::Idle)
        return;
    m_startTimeout.stop();
    setStage(Stage::Done);
    setStage(Stage::Idle);
    emit finished(true, QString());
}

void UpdateManager::cancel()
{
    if (m_stage == Stage::Idle)
        return;
    m_runner->cancel(); // 当前命令被杀后回调会因 m_stage==Idle 直接返回
    fail(QStringLiteral("更新已取消"));
}

} // namespace dshinqt
