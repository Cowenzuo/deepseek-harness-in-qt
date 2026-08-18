#pragma once

#include <QMainWindow>

#include "process/dshprocessmanager.h"
#include "settings/appsettings.h"
#include "update/updatemanager.h"

class QStackedWidget;
class QCloseEvent;
class QLabel;
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
    void openSettings();
    void onDshStateChanged(DshProcessManager::State state);
    void onDshLog(const QString &line, bool isError);
    void onSetupFinished();
    void onErrorRetry();
    void onErrorBuild();
    void onUpdateStageChanged(UpdateManager::Stage stage);
    void onUpdateFinished(bool success, const QString &error);

private:
    void continueToService();
    void startService();
    void runOneClickBuild(bool fromSetup = false);
    void runBuildStep(const QStringList &pnpmArgs);
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
    QLabel *m_statusLabel = nullptr; // 状态栏右侧状态消息（替代 showMessage）
    bool m_buildFromSetup = false;
};
