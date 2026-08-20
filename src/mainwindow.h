#pragma once

#include <QFutureWatcher>
#include <QMainWindow>

#include "process/buildflowmanager.h"
#include "process/buildstaleness.h"
#include "process/dshprocessmanager.h"
#include "settings/appsettings.h"
#include "update/updatemanager.h"

class QStackedWidget;
class QCloseEvent;
class QLabel;

namespace dshinqt {

class GitClient;
class HomePage;
class LogView;
class PreflightChecker;
class EnvironmentChecker;
class SetupPage;
class StartupPage;
class ErrorPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void showHomePage();
    void showLogPage();
    void openSettings(int page = 0); // page：设置弹窗初始导航页（0=常规，3=修复）
    void onDshStateChanged(DshProcessManager::State state);
    void onDshLog(const QString &line, bool isError);
    void onSetupFinished();
    void onErrorRetry();
    void onErrorBuild();
    void onBuildFinished(bool success, const QString &error, BuildFlowManager::Origin origin);
    void onUpdateStageChanged(UpdateManager::Stage stage);
    void onUpdateFinished(bool success, const QString &error);
    void onStaleCheckFinished(); // 启动前构建产物自检完成（后台 git 读取）

private:
    void continueToService();
    void startService();
    void checkStaleBuildAndProceed(); // 外部 git pull 后产物过期 → 提示重建再启动
    void runOneClickBuild(BuildFlowManager::Origin origin);
    void startClone();
    void showSetupPage();
    void showErrorPage(const QString &message, bool canBuild);

    AppSettings m_settings;
    QStackedWidget *m_pages = nullptr;
    StartupPage *m_startupPage = nullptr;
    SetupPage *m_setupPage = nullptr;
    HomePage *m_homePage = nullptr;
    ErrorPage *m_errorPage = nullptr;
    LogView *m_logView = nullptr;
    DshProcessManager *m_process = nullptr;
    PreflightChecker *m_preflight = nullptr;
    EnvironmentChecker *m_env = nullptr;
    GitClient *m_git = nullptr;
    UpdateManager *m_update = nullptr;
    BuildFlowManager *m_buildFlow = nullptr;
    QFutureWatcher<BuildStalenessInfo> *m_staleWatcher = nullptr; // 启动前产物自检（后台 git）
    QLabel *m_statusLabel = nullptr; // 状态栏右侧状态消息（替代 showMessage）
};

} // namespace dshinqt
