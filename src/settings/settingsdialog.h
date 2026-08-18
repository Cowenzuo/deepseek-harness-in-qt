#pragma once

#include <QDialog>
#include <QFutureWatcher>
#include <QList>

#include "git/gitclient.h"
#include "update/updatemanager.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTreeWidget;

namespace dshinqt {

class AppSettings;
class DshProcessManager;
class UpdateManager;

// 后台线程一次性采集的仓库快照，避免同步 git 命令阻塞 UI
struct RepoSnapshot
{
    QString branch;
    bool aheadValid = false;
    int ahead = 0;
    int behind = 0;
    bool dirty = false;
    QList<GitBranch> branches;
    QList<GitCommit> commits;
};

// Fetch 后台执行结果
struct FetchResult
{
    bool ok = false;
    QString err;
};

// 统一设置弹窗：左侧竖向导航（常规 / 服务 / 更新 / 关于）+ 右侧页面。
// 更新页 = 实用的 git 页面：分支树 + 提交列表，快速查看、
// 选定分支/提交切换或更新当前分支。全部一页内完成，不弹任何子窗口。
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(AppSettings *settings, GitClient *git, UpdateManager *update, DshProcessManager *proc,
                            QWidget *parent = nullptr);
    ~SettingsDialog() override; // 等待后台 git 线程结束，避免访问已析构对象

private slots:
    void saveSettings();
    void refreshRepo();         // 异步发起仓库信息刷新（不阻塞 UI）
    void onRepoSnapshotReady(); // 后台采集完成，更新 UI
    void onFetch();
    void onUpdateCurrentBranch();
    void onSwitchBranch();           // 切换到分支树中选中的分支
    void onSwitchCommitSelected();   // 切换到提交列表选中的提交
    void onBranchTreeChanged();      // 点击分支 → 异步加载该分支提交
    void onBranchCommitsReady();     // 分支提交加载完成
    void onFetchFinished();          // 后台 fetch 完成，更新 UI
    void onCommitActivated(int row); // 双击提交列表行 → 切换到此提交
    void onNavChanged(int row);      // 左侧导航切换页面
    void onStartService();
    void onStopService();
    void onRestartService();

private:
    QWidget *buildGeneralTab();
    QWidget *buildServiceTab();
    QWidget *buildRepoUpdateTab();
    QWidget *buildAboutTab();
    QPushButton *makeButton(const QString &text);
    void populateCommits();
    void populateBranchTree(const QList<GitBranch> &branches);
    void loadBranchCommits(const QString &rev);
    void refreshServiceUi();
    bool confirmServiceInterrupt(const QString &action); // 危险操作确认（停止/重启/更新）
    void beginUpdate(const UpdateManager::Target &target); // 更新统一入口（含确认）

    AppSettings *m_settings = nullptr;
    GitClient *m_git = nullptr;
    UpdateManager *m_update = nullptr;
    DshProcessManager *m_proc = nullptr;
    QFutureWatcher<RepoSnapshot> *m_watcher = nullptr;
    QFutureWatcher<QList<GitCommit>> *m_commitWatcher = nullptr;
    QFutureWatcher<FetchResult> *m_fetchWatcher = nullptr;

    // 左侧导航 + 页面容器
    QListWidget *m_nav = nullptr;
    QStackedWidget *m_pages = nullptr;

    // 常规设置
    QLineEdit *m_sourcePathEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QLineEdit *m_nodePathEdit = nullptr;
    QLineEdit *m_pnpmPathEdit = nullptr;
    QLineEdit *m_gitPathEdit = nullptr;
    QLineEdit *m_repoUrlEdit = nullptr;

    // 更新页
    QLabel *m_branchLabel = nullptr;     // 顶栏当前分支
    QLabel *m_statusLabel = nullptr;     // 顶栏同步状态（领先/落后/同步/无上游/错误）
    QTreeWidget *m_branchTree = nullptr; // 分支树（本地/远程分组）
    QListWidget *m_commitList = nullptr; // 提交列表
    QList<GitCommit> m_commits;

    // 服务页
    QLabel *m_svcStatusLabel = nullptr; // 状态灯
    QLabel *m_svcPidLabel = nullptr;    // PID
    QLabel *m_svcSourceLabel = nullptr; // 源码路径
    QPushButton *m_svcStartBtn = nullptr;
    QPushButton *m_svcStopBtn = nullptr;
    QPushButton *m_svcRestartBtn = nullptr;
};

} // namespace dshinqt
