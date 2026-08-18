#include "repoupdatepage.h"

#include <QtConcurrent>

#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "appsettings.h"

namespace dshinqt {

namespace {
const int kCommitPageSize = 60; // 提交列表每批条数

// 后台线程执行：采集仓库信息。不捕获页面 this，仅使用 GitClient 指针（生命周期长于页面）
// 与 sourcePath 快照，避免 use-after-free 与跨线程访问 AppSettings。
RepoSnapshot collectSnapshot(GitClient *git, const QString &sourcePath)
{
    RepoSnapshot s;
    s.branch = git->currentBranch(sourcePath);
    int ahead = 0, behind = 0;
    s.aheadValid = git->aheadBehind(&ahead, &behind, sourcePath);
    s.ahead = ahead;
    s.behind = behind;
    s.dirty = git->isDirty(sourcePath);
    s.branches = git->branches(sourcePath);
    s.commits = git->commits(kCommitPageSize, 0, sourcePath);
    return s;
}

QString stageText(UpdateManager::Stage s)
{
    switch (s) {
    case UpdateManager::Stage::Stopping: return QStringLiteral("停止服务");
    case UpdateManager::Stage::Fetch: return QStringLiteral("拉取");
    case UpdateManager::Stage::Checkout: return QStringLiteral("切换");
    case UpdateManager::Stage::Pull: return QStringLiteral("合并更新");
    case UpdateManager::Stage::Install: return QStringLiteral("装依赖");
    case UpdateManager::Stage::Build: return QStringLiteral("构建");
    case UpdateManager::Stage::Starting: return QStringLiteral("启动");
    case UpdateManager::Stage::Done: return QStringLiteral("完成");
    case UpdateManager::Stage::Failed: return QStringLiteral("失败");
    case UpdateManager::Stage::Idle: break;
    }
    return {};
}
} // namespace

RepoUpdatePage::RepoUpdatePage(AppSettings *settings, GitClient *git, UpdateManager *update, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_git(git)
    , m_update(update)
    , m_watcher(new QFutureWatcher<RepoSnapshot>(this))
    , m_commitWatcher(new QFutureWatcher<QList<GitCommit>>(this))
    , m_fetchWatcher(new QFutureWatcher<FetchResult>(this))
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(24, 20, 24, 20);
    v->setSpacing(14);

    // —— 顶栏一行：当前分支 + 同步状态 + 操作按钮 ——
    m_branchLabel = new QLabel(this);
    m_branchLabel->setStyleSheet(QStringLiteral("color:#f5f5f5; font-size:15px; font-weight:700;"));
    m_statusLabel = new QLabel(this);
    m_statusLabel->setTextFormat(Qt::RichText);
    m_statusLabel->setStyleSheet(QStringLiteral("font-size:13px;"));
    m_stageLabel = new QLabel(this);
    m_stageLabel->setTextFormat(Qt::RichText);
    m_stageLabel->setVisible(false);

    const auto makeBtn = [this](const QString &text) {
        auto *b = new QPushButton(text, this);
        b->setObjectName(QStringLiteral("secondary"));
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::TabFocus);
        return b;
    };
    m_fetchBtn = makeBtn(QStringLiteral("Fetch 刷新"));
    m_updateBtn = makeBtn(QStringLiteral("更新当前分支"));
    m_cancelBtn = makeBtn(QStringLiteral("取消更新"));
    m_cancelBtn->setVisible(false);
    connect(m_fetchBtn, &QPushButton::clicked, this, &RepoUpdatePage::onFetch);
    connect(m_updateBtn, &QPushButton::clicked, this, &RepoUpdatePage::onUpdateCurrentBranch);
    connect(m_cancelBtn, &QPushButton::clicked, m_update, &UpdateManager::cancel);

    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(12);
    topRow->addWidget(m_branchLabel);
    topRow->addWidget(m_statusLabel);
    topRow->addWidget(m_stageLabel);
    topRow->addStretch(1);
    topRow->addWidget(m_fetchBtn);
    topRow->addWidget(m_updateBtn);
    topRow->addWidget(m_cancelBtn);
    v->addLayout(topRow);

    // —— 左：分支树 ——
    auto *branchCard = new QFrame(this);
    branchCard->setObjectName(QStringLiteral("card"));
    branchCard->setFixedWidth(250);
    auto *bc = new QVBoxLayout(branchCard);
    bc->setContentsMargins(16, 14, 16, 14);
    bc->setSpacing(10);
    auto *branchTitle = new QLabel(QStringLiteral("分支"), branchCard);
    branchTitle->setObjectName(QStringLiteral("pageTitle"));
    m_switchBranchBtn = makeBtn(QStringLiteral("切换到该分支"));
    connect(m_switchBranchBtn, &QPushButton::clicked, this, &RepoUpdatePage::onSwitchBranch);
    auto *branchTitleRow = new QHBoxLayout;
    branchTitleRow->addWidget(branchTitle);
    branchTitleRow->addStretch(1);
    branchTitleRow->addWidget(m_switchBranchBtn);
    m_branchTree = new QTreeWidget(branchCard);
    m_branchTree->setHeaderHidden(true);
    m_branchTree->setFocusPolicy(Qt::TabFocus);
    m_branchTree->setColumnCount(1);
    connect(m_branchTree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *cur, QTreeWidgetItem *) {
        Q_UNUSED(cur);
        onBranchTreeChanged();
    });
    connect(m_branchTree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
        m_branchTree->setCurrentItem(item);
        onSwitchBranch();
    });
    bc->addLayout(branchTitleRow);
    bc->addWidget(m_branchTree, 1);

    // —— 右：提交列表 ——
    auto *commitCard = new QFrame(this);
    commitCard->setObjectName(QStringLiteral("card"));
    auto *cc = new QVBoxLayout(commitCard);
    cc->setContentsMargins(16, 14, 16, 14);
    cc->setSpacing(10);
    auto *commitTitle = new QLabel(QStringLiteral("提交记录（双击行切换到此提交）"), commitCard);
    commitTitle->setObjectName(QStringLiteral("pageTitle"));
    m_switchCommitBtn = makeBtn(QStringLiteral("切换到该提交"));
    connect(m_switchCommitBtn, &QPushButton::clicked, this, &RepoUpdatePage::onSwitchCommitSelected);
    auto *commitTitleRow = new QHBoxLayout;
    commitTitleRow->addWidget(commitTitle);
    commitTitleRow->addStretch(1);
    commitTitleRow->addWidget(m_switchCommitBtn);
    m_commitList = new QListWidget(commitCard);
    m_commitList->setFocusPolicy(Qt::TabFocus);
    connect(m_commitList, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        onCommitActivated(m_commitList->row(item));
    });
    cc->addLayout(commitTitleRow);
    cc->addWidget(m_commitList, 1);

    auto *mainRow = new QHBoxLayout;
    mainRow->setSpacing(16);
    mainRow->addWidget(branchCard);
    mainRow->addWidget(commitCard, 1);
    v->addLayout(mainRow, 1);

    connect(m_watcher, &QFutureWatcher<RepoSnapshot>::finished, this, &RepoUpdatePage::onRepoSnapshotReady);
    connect(m_commitWatcher, &QFutureWatcher<QList<GitCommit>>::finished, this,
            &RepoUpdatePage::onBranchCommitsReady);
    connect(m_fetchWatcher, &QFutureWatcher<FetchResult>::finished, this, &RepoUpdatePage::onFetchFinished);
    // 更新流水线阶段反馈：更新期间禁用操作按钮并显示阶段文字
    connect(m_update, &UpdateManager::stageChanged, this, [this](UpdateManager::Stage s) {
        const bool busy = (s != UpdateManager::Stage::Idle);
        m_fetchBtn->setEnabled(!busy);
        m_updateBtn->setEnabled(!busy);
        m_switchBranchBtn->setEnabled(!busy);
        m_switchCommitBtn->setEnabled(!busy);
        m_cancelBtn->setVisible(busy);
        m_stageLabel->setVisible(busy);
        if (busy)
            m_stageLabel->setText(QStringLiteral("<span style='color:#e0a030;'>● 更新中：%1...</span>")
                                      .arg(stageText(s)));
    });

    refreshRepo();
}

RepoUpdatePage::~RepoUpdatePage()
{
    // 等待后台 git 线程结束：线程不捕获 this（仅用 GitClient 指针与快照），
    // 但写配置前串行化可杜绝与保存配置的并发访问。
    waitForBackgroundTasks();
}

void RepoUpdatePage::waitForBackgroundTasks()
{
    m_watcher->waitForFinished();
    m_commitWatcher->waitForFinished();
    m_fetchWatcher->waitForFinished();
}

void RepoUpdatePage::refreshRepo()
{
    if (m_watcher->isRunning())
        return; // 上一次刷新仍在后台进行
    m_branchLabel->setText(QStringLiteral("…"));
    m_statusLabel->setText(QStringLiteral("<span style='color:#9a9a9a;'>● 正在刷新仓库信息...</span>"));
    m_branchTree->clear();
    m_commitList->clear();
    const QString src = m_settings->sourcePath;
    GitClient *git = m_git;
    m_watcher->setFuture(QtConcurrent::run([git, src] { return collectSnapshot(git, src); }));
}

void RepoUpdatePage::onRepoSnapshotReady()
{
    const RepoSnapshot s = m_watcher->result();

    m_branchLabel->setText(s.branch.isEmpty() ? QStringLiteral("（未检出分支）") : s.branch);

    if (s.aheadValid) {
        if (s.behind > 0)
            m_statusLabel->setText(
                QStringLiteral("<span style='color:#e0a030;'>● 落后 %1 个提交，有更新可拉取</span>").arg(s.behind));
        else if (s.ahead > 0)
            m_statusLabel->setText(QStringLiteral("<span style='color:#7ab0ff;'>● 领先 %1 个提交</span>").arg(s.ahead));
        else
            m_statusLabel->setText(QStringLiteral("<span style='color:#4caf50;'>● 已同步</span>"));
    } else {
        m_statusLabel->setText(QStringLiteral("<span style='color:#787878;'>● 无上游分支</span>"));
    }
    if (s.dirty) {
        m_statusLabel->setText(m_statusLabel->text() +
                               QStringLiteral(" <span style='color:#e06060;'>· 工作区有改动</span>"));
    }

    m_commits = s.commits; // HEAD 的提交，先展示当前分支
    populateBranchTree(s.branches);
    populateCommits();
}

void RepoUpdatePage::populateCommits()
{
    m_commitList->clear();
    for (const auto &c : m_commits) {
        m_commitList->addItem(QStringLiteral("● %1  %2    %3 · %4").arg(c.hash.left(7), c.message, c.author, c.date));
    }
}

void RepoUpdatePage::populateBranchTree(const QList<GitBranch> &branches)
{
    m_branchTree->clear();
    auto *localRoot = new QTreeWidgetItem(m_branchTree, {QStringLiteral("本地分支")});
    localRoot->setFlags(localRoot->flags() & ~Qt::ItemIsSelectable);
    auto *remoteRoot = new QTreeWidgetItem(m_branchTree, {QStringLiteral("远程分支")});
    remoteRoot->setFlags(remoteRoot->flags() & ~Qt::ItemIsSelectable);

    QString currentName;
    for (const auto &b : branches) {
        auto *item = new QTreeWidgetItem(b.isRemote ? remoteRoot : localRoot, {b.name});
        item->setData(0, Qt::UserRole, b.name);
        if (b.isCurrent) {
            currentName = b.name;
            QFont f = item->font(0);
            f.setBold(true);
            item->setFont(0, f);
        }
    }
    m_branchTree->expandAll();

    // 选中当前分支（blockSignals 避免重复触发提交加载，HEAD 提交已填充）
    if (!currentName.isEmpty()) {
        m_branchTree->blockSignals(true);
        for (int i = 0; i < m_branchTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *root = m_branchTree->topLevelItem(i);
            for (int j = 0; j < root->childCount(); ++j) {
                QTreeWidgetItem *it = root->child(j);
                if (it->data(0, Qt::UserRole).toString() == currentName) {
                    m_branchTree->setCurrentItem(it);
                    break;
                }
            }
        }
        m_branchTree->blockSignals(false);
    }
}

void RepoUpdatePage::loadBranchCommits(const QString &rev)
{
    if (m_commitWatcher->isRunning())
        return;
    m_commitList->clear();
    m_commitList->addItem(QStringLiteral("加载中..."));
    const QString src = m_settings->sourcePath;
    GitClient *git = m_git;
    m_commitWatcher->setFuture(QtConcurrent::run([git, rev, src] { return git->commits(rev, kCommitPageSize, 0, src); }));
}

void RepoUpdatePage::onBranchTreeChanged()
{
    QTreeWidgetItem *item = m_branchTree->currentItem();
    if (!item)
        return;
    const QString name = item->data(0, Qt::UserRole).toString();
    if (name.isEmpty())
        return; // 组节点
    loadBranchCommits(name);
}

void RepoUpdatePage::onBranchCommitsReady()
{
    m_commits = m_commitWatcher->result();
    populateCommits();
}

void RepoUpdatePage::onFetch()
{
    if (m_fetchWatcher->isRunning())
        return;
    m_statusLabel->setText(QStringLiteral("<span style='color:#9a9a9a;'>● Fetching...</span>"));
    const QString src = m_settings->sourcePath;
    GitClient *git = m_git;
    m_fetchWatcher->setFuture(QtConcurrent::run([git, src]() {
        FetchResult r;
        r.ok = git->fetch(&r.err, src);
        return r;
    }));
}

void RepoUpdatePage::onFetchFinished()
{
    const FetchResult r = m_fetchWatcher->result();
    if (!r.ok) {
        m_statusLabel->setText(
            QStringLiteral("<span style='color:#e06060;'>● Fetch 失败：%1</span>").arg(r.err.toHtmlEscaped()));
        return;
    }
    refreshRepo();
}

void RepoUpdatePage::onCommitActivated(int row)
{
    if (row < 0 || row >= m_commits.size())
        return;
    const QString hash = m_commits[row].hash;
    UpdateManager::Target t;
    t.kind = UpdateManager::Target::Commit;
    t.value = hash;
    beginUpdate(t);
}

void RepoUpdatePage::onUpdateCurrentBranch()
{
    const QString branch = m_git->currentBranch();
    if (branch.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("更新"), QStringLiteral("无法获取当前分支"));
        return;
    }
    UpdateManager::Target t;
    t.kind = UpdateManager::Target::Branch;
    t.value = branch;
    beginUpdate(t);
}

void RepoUpdatePage::onSwitchBranch()
{
    QTreeWidgetItem *item = m_branchTree->currentItem();
    if (!item)
        return;
    const QString name = item->data(0, Qt::UserRole).toString();
    if (name.isEmpty() || name == m_git->currentBranch())
        return; // 组节点 / 已是该分支，无需切换
    UpdateManager::Target t;
    t.kind = UpdateManager::Target::Branch;
    t.value = name;
    beginUpdate(t);
}

void RepoUpdatePage::onSwitchCommitSelected()
{
    onCommitActivated(m_commitList->currentRow());
}

void RepoUpdatePage::beginUpdate(const UpdateManager::Target &target)
{
    if (QMessageBox::question(this, QStringLiteral("确认"),
                              QStringLiteral("更新将停止服务、切换版本并重新构建（可能需数分钟），"
                                             "期间对话会中断。继续？"),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;
    m_update->start(target); // 更新在后台执行，对话框保留
}

} // namespace dshinqt
