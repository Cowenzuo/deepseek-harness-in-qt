#pragma once

#include <QFutureWatcher>
#include <QList>
#include <QWidget>

#include "git/gitclient.h"
#include "update/updatemanager.h"

class QLabel;
class QListWidget;
class QPushButton;
class QTreeWidget;

namespace dshinqt {

class AppSettings;

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

// 设置弹窗「更新」页：Git 查看（分支树/提交列表/同步状态）+ 更新触发（含确认与阶段反馈/取消）。
class RepoUpdatePage : public QWidget
{
    Q_OBJECT

public:
    explicit RepoUpdatePage(AppSettings *settings, GitClient *git, UpdateManager *update, QWidget *parent = nullptr);
    ~RepoUpdatePage() override;

    void refreshRepo();                // 异步发起仓库信息刷新（保存配置后由壳调用）
    void waitForBackgroundTasks();     // 等待在途后台 git 线程（保存配置前由壳调用）

private slots:
    void onRepoSnapshotReady();
    void onFetch();
    void onFetchFinished();
    void onUpdateCurrentBranch();
    void onSwitchBranch();
    void onSwitchCommitSelected();
    void onBranchTreeChanged();
    void onBranchCommitsReady();
    void onCommitActivated(int row);

private:
    void populateCommits();
    void populateBranchTree(const QList<GitBranch> &branches);
    void loadBranchCommits(const QString &rev);
    void beginUpdate(const UpdateManager::Target &target);

    AppSettings *m_settings = nullptr;
    GitClient *m_git = nullptr;
    UpdateManager *m_update = nullptr;
    QFutureWatcher<RepoSnapshot> *m_watcher = nullptr;
    QFutureWatcher<QList<GitCommit>> *m_commitWatcher = nullptr;
    QFutureWatcher<FetchResult> *m_fetchWatcher = nullptr;

    QLabel *m_branchLabel = nullptr;     // 顶栏当前分支
    QLabel *m_statusLabel = nullptr;     // 顶栏同步状态（领先/落后/同步/无上游/错误）
    QLabel *m_stageLabel = nullptr;      // 更新进行中的阶段文字（空闲时隐藏）
    QTreeWidget *m_branchTree = nullptr; // 分支树（本地/远程分组）
    QListWidget *m_commitList = nullptr; // 提交列表
    QList<GitCommit> m_commits;
    QPushButton *m_fetchBtn = nullptr;        // Fetch 刷新
    QPushButton *m_updateBtn = nullptr;       // 更新当前分支
    QPushButton *m_switchBranchBtn = nullptr; // 切换到该分支
    QPushButton *m_switchCommitBtn = nullptr; // 切换到该提交
    QPushButton *m_cancelBtn = nullptr;       // 取消更新（更新期间显示）
};

} // namespace dshinqt
