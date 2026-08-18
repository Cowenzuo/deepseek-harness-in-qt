#pragma once

#include <QObject>
#include <QTimer>

#include "process/dshprocessmanager.h"

namespace dshinqt {

class AppSettings;
class GitClient;

class UpdateManager : public QObject
{
    Q_OBJECT

public:
    enum class Stage { Idle, Stopping, Fetch, Checkout, Pull, Install, Build, Starting, Done, Failed };
    Q_ENUM(Stage)

    struct Target
    {
        enum Kind { Branch, Commit } kind = Branch;
        QString value;
    };

    UpdateManager(AppSettings *settings, GitClient *git, DshProcessManager *proc, QObject *parent = nullptr);

    void start(const Target &target);

signals:
    void stageChanged(UpdateManager::Stage stage);
    void logOutput(const QString &line, bool isError);
    void finished(bool success, const QString &error);

private slots:
    void onProcStateChanged(DshProcessManager::State state);

private:
    void setStage(Stage s);
    void runGitFetch();
    void beginCheckout();
    void runGitPull();
    void beginInstall();
    void beginBuild();
    void beginStart();
    void runPnpm(const QStringList &args, Stage nextStage);
    void fail(const QString &error);
    void done();

    AppSettings *m_settings = nullptr;
    GitClient *m_git = nullptr;
    DshProcessManager *m_proc = nullptr;
    QTimer m_startTimeout; // Starting 阶段兜底：60s 内服务未 Running 则按失败处理
    Target m_target;
    Stage m_stage = Stage::Idle;
};

} // namespace dshinqt
