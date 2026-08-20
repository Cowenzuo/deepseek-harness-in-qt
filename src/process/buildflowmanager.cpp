#include "buildflowmanager.h"

#include <QProcessEnvironment>

#include "commandrunner.h"
#include "proxydetector.h"
#include "settings/appsettings.h"

namespace dshinqt {

BuildFlowManager::BuildFlowManager(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    m_runner = new CommandRunner(this);
    connect(m_runner, &CommandRunner::outputReady, this, [this](const QString &text) {
        emit logOutput(text, false);
    });
}

void BuildFlowManager::startOneClickBuild(Origin origin)
{
    if (m_busy) {
        emit logOutput(QStringLiteral("[构建] 已有构建任务进行中，本次请求已忽略"), false);
        return;
    }
    m_origin = origin;
    m_busy = true;
    beginInstall();
}

void BuildFlowManager::startClone()
{
    if (m_busy) {
        emit logOutput(QStringLiteral("[构建] 已有构建任务进行中，本次请求已忽略"), false);
        return;
    }
    m_origin = Origin::SetupPage; // 克隆仅在引导页发起
    m_busy = true;

    const QString url = m_settings->repoUrl.isEmpty()
                            ? QStringLiteral("https://github.com/deepseek-ai/deepseek-harness.git")
                            : m_settings->repoUrl;
    const QString target = m_settings->sourcePath;

    emit logOutput(QStringLiteral("> git clone --progress %1 %2").arg(url, target), false);
    m_runner->start(m_settings->gitProgram(),
                    ProxyDetector::gitProxyArgs()
                        + QStringList{QStringLiteral("clone"), QStringLiteral("--progress"), url, target},
                    QString(), QProcessEnvironment(),
                    [this](bool success, int code) {
                        if (!success) {
                            emit logOutput(QStringLiteral("git clone 失败（code=%1）").arg(code), true);
                            finish(false, QStringLiteral("git clone 失败，请查看日志页确认原因（网络/仓库地址）。"));
                            return;
                        }
                        emit logOutput(QStringLiteral("克隆完成，开始安装依赖..."), false);
                        beginInstall();
                    });
}

void BuildFlowManager::beginInstall()
{
    emit logOutput(QStringLiteral("> pnpm install"), false);
    runPnpmStep({QStringLiteral("install")}, [this](bool success, int code) {
        if (!success) {
            finish(false, QStringLiteral("pnpm install 失败，请查看日志页确认原因。"));
            return;
        }
        beginBuild();
    });
}

void BuildFlowManager::beginBuild()
{
    emit logOutput(QStringLiteral("> pnpm run build"), false);
    runPnpmStep({QStringLiteral("run"), QStringLiteral("build")}, [this](bool success, int code) {
        if (success)
            emit logOutput(QStringLiteral("构建完成"), false);
        finish(success, success ? QString()
                                : QStringLiteral("pnpm run build 失败，请查看日志页确认原因。"));
    });
}

void BuildFlowManager::runPnpmStep(const QStringList &args, std::function<void(bool success, int code)> onExit)
{
    const QString pnpm = m_settings->pnpmPath.isEmpty() ? QStringLiteral("pnpm") : m_settings->pnpmPath;
    // startWrapped 处理 .cmd shim 包装；代理环境由 pnpmEnvironment 注入
    m_runner->start(pnpm, args, m_settings->sourcePath, ProxyDetector::pnpmEnvironment(),
                    [this, args, onExit](bool success, int code) {
                        if (!success)
                            emit logOutput(QStringLiteral("命令失败：pnpm %1（code=%2）").arg(args.join(' ')).arg(code),
                                           true);
                        onExit(success, code);
                    });
}

void BuildFlowManager::finish(bool success, const QString &error)
{
    m_busy = false;
    emit buildFinished(success, error, m_origin);
}

} // namespace dshinqt
