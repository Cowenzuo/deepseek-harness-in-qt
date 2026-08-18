#include "updatemanager.h"

#include <QProcess>

#include "git/gitclient.h"
#include "process/proxydetector.h"
#include "process/startwrapped.h"
#include "settings/appsettings.h"

UpdateManager::UpdateManager(AppSettings *settings, GitClient *git, DshProcessManager *proc, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_git(git)
    , m_proc(proc)
{
    connect(m_proc, &DshProcessManager::stateChanged, this, &UpdateManager::onProcStateChanged);
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

void UpdateManager::runGitFetch()
{
    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(m_settings->sourcePath);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, &QProcess::readyReadStandardOutput, this, [proc, this]() {
        emit logOutput(QString::fromUtf8(proc->readAllStandardOutput()), false);
    });
    connect(proc, &QProcess::finished, this, [proc, this](int code, QProcess::ExitStatus status) {
        proc->deleteLater();
        if (status != QProcess::NormalExit || code != 0) {
            fail(QStringLiteral("git fetch 失败（code=%1）").arg(code));
            return;
        }
        setStage(Stage::Checkout);
        beginCheckout();
    });

    proc->start(QStringLiteral("git"),
                ProxyDetector::gitProxyArgs() +
                    QStringList{QStringLiteral("fetch"), QStringLiteral("--all"), QStringLiteral("--prune")});
}

void UpdateManager::beginCheckout()
{
    QString err;
    const bool ok = (m_target.kind == Target::Branch) ? m_git->checkoutBranch(m_target.value, &err)
                                                      : m_git->checkoutCommit(m_target.value, &err);
    if (!ok) {
        fail(err);
        return;
    }
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

void UpdateManager::runGitPull()
{
    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(m_settings->sourcePath);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, &QProcess::readyReadStandardOutput, this, [proc, this]() {
        emit logOutput(QString::fromUtf8(proc->readAllStandardOutput()), false);
    });
    connect(proc, &QProcess::finished, this, [proc, this](int code, QProcess::ExitStatus status) {
        proc->deleteLater();
        if (status != QProcess::NormalExit || code != 0) {
            fail(QStringLiteral("git pull 失败（code=%1）").arg(code));
            return;
        }
        setStage(Stage::Install);
        beginInstall();
    });

    proc->start(QStringLiteral("git"),
                ProxyDetector::gitProxyArgs() + QStringList{QStringLiteral("pull"), QStringLiteral("--ff-only")});
}

void UpdateManager::beginInstall()
{
    setStage(Stage::Install);
    emit logOutput(QStringLiteral("> pnpm install"), false);
    runPnpm({QStringLiteral("install")}, Stage::Build);
}

void UpdateManager::beginBuild()
{
    setStage(Stage::Build);
    emit logOutput(QStringLiteral("> pnpm run build"), false);
    runPnpm({QStringLiteral("run"), QStringLiteral("build")}, Stage::Starting);
}

void UpdateManager::beginStart()
{
    setStage(Stage::Starting);
    emit logOutput(QStringLiteral("重新启动 dsh..."), false);
    m_proc->start();
}

void UpdateManager::runPnpm(const QStringList &args, Stage nextStage)
{
    const QString pnpm = m_settings->pnpmPath.isEmpty() ? QStringLiteral("pnpm") : m_settings->pnpmPath;

    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(m_settings->sourcePath);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, &QProcess::readyReadStandardOutput, this, [proc, this]() {
        emit logOutput(QString::fromUtf8(proc->readAllStandardOutput()), false);
    });
    connect(proc, &QProcess::finished, this, [proc, nextStage, this](int code, QProcess::ExitStatus status) {
        proc->deleteLater();
        if (status != QProcess::NormalExit || code != 0) {
            fail(QStringLiteral("pnpm 命令失败（code=%1）").arg(code));
            return;
        }
        if (nextStage == Stage::Build)
            beginBuild();
        else if (nextStage == Stage::Starting)
            beginStart();
    });

    startWrapped(proc, pnpm, args);
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
    setStage(Stage::Failed);
    emit logOutput(error, true);
    setStage(Stage::Idle); // 先复位再通知结果，避免槽内同步发起新更新被 Idle 守卫拒绝
    emit finished(false, error);
}

void UpdateManager::done()
{
    setStage(Stage::Done);
    setStage(Stage::Idle);
    emit finished(true, QString());
}
