#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace dshinqt {

class AppSettings;
class CommandRunner;

// 引导/错误页的构建流水线：克隆仓库 → pnpm install → pnpm run build（QProcess 异步串联）。
// 从 MainWindow 拆出：MainWindow 只负责页面切换与结果分派。
class BuildFlowManager : public QObject
{
    Q_OBJECT

public:
    // 发起来源：决定完成后回引导页（SetupPage）、启动服务（ErrorPage/Startup）还是报错页
    enum class Origin { SetupPage, ErrorPage, Startup };
    Q_ENUM(Origin)

    explicit BuildFlowManager(AppSettings *settings, QObject *parent = nullptr);

    bool isBusy() const { return m_busy; }

    void startOneClickBuild(Origin origin); // install → build
    void startClone();                      // clone → install → build（仅引导页发起）

signals:
    void logOutput(const QString &line, bool isError);
    void buildFinished(bool success, const QString &error, BuildFlowManager::Origin origin);

private:
    void beginInstall();
    void beginBuild();
    void runPnpmStep(const QStringList &args, std::function<void(bool success, int code)> onExit);
    void finish(bool success, const QString &error);

    AppSettings *m_settings = nullptr;
    CommandRunner *m_runner = nullptr;
    Origin m_origin = Origin::SetupPage;
    bool m_busy = false;
};

} // namespace dshinqt
