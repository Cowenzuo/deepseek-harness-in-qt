#include "settingsdialog.h"

#include "appsettings.h"
#include "git/gitclient.h"
#include "process/dshprocessmanager.h"
#include "update/updatemanager.h"

#include <QtConcurrent>

#include <QCoreApplication>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSize>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {
QLabel *fieldTitle(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("fieldTitle"));
    return l;
}
} // namespace

SettingsDialog::SettingsDialog(AppSettings *settings, GitClient *git, UpdateManager *update, DshProcessManager *proc,
                               QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_git(git)
    , m_update(update)
    , m_proc(proc)
    , m_watcher(new QFutureWatcher<RepoSnapshot>(this))
    , m_commitWatcher(new QFutureWatcher<QList<GitCommit>>(this))
{
    setWindowTitle(QStringLiteral("设置"));
    setModal(true);
    resize(900, 620);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint); // 支持最大化

    connect(m_watcher, &QFutureWatcher<RepoSnapshot>::finished, this, &SettingsDialog::onRepoSnapshotReady);
    connect(m_commitWatcher, &QFutureWatcher<QList<GitCommit>>::finished, this, &SettingsDialog::onBranchCommitsReady);
    connect(m_proc, &DshProcessManager::stateChanged, this, [this](DshProcessManager::State) { refreshServiceUi(); });

    // 深色卡片式样式（与引导页一致）
    setStyleSheet(QStringLiteral(R"(
QDialog { background: #121212; }
QStackedWidget {
    background: #18181c; border: 1px solid #26262b; border-radius: 10px;
}
QListWidget#nav {
    background: #18181c; border: none; border-right: 1px solid #26262b;
    outline: none; padding: 18px 8px; font-size: 12px;
}
QListWidget#nav::item {
    color: #9a9aa0; background: #222227; border: 1px solid #2e2e34;
    border-radius: 9px; margin: 6px 2px; padding: 12px 2px;
}
QListWidget#nav::item:hover { background: #2a2a30; color: #d6d6db; }
QListWidget#nav::item:selected {
    background: #2f3550; border: 1px solid #4f8cff; color: #ffffff;
}
QLabel#pageTitle { color: #f5f5f5; font-size: 15px; font-weight: 700; }
QLabel#fieldTitle { color: #b5b5b5; font-size: 12px; font-weight: 600; }
QLineEdit, QSpinBox, QComboBox {
    background: #26262a; border: 1px solid #38383e; border-radius: 8px;
    padding: 8px 12px; color: #e8e8e8; font-size: 13px;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border: 1px solid #4f8cff; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox QAbstractItemView {
    background: #1d1d1f; border: 1px solid #38383e; border-radius: 6px;
    color: #e8e8e8; selection-background-color: #2f3550; outline: none;
}
QFrame#card { background: #1d1d1f; border: 1px solid #2b2b2e; border-radius: 10px; }
QPushButton#primary {
    background: #4f8cff; color: #ffffff; border: none; border-radius: 8px;
    padding: 10px 28px; font-size: 13px; font-weight: 700;
}
QPushButton#primary:hover { background: #4077e0; }
QPushButton#primary:pressed { background: #3566c4; }
QPushButton#secondary {
    background: #2c2c31; border: 1px solid #4f8cff; border-radius: 8px;
    padding: 9px 18px; color: #7ab0ff; font-size: 13px; font-weight: 600;
}
QPushButton#secondary:hover { background: #35353b; color: #ffffff; }
QPushButton#secondary:pressed { background: #2f3550; }
QListWidget {
    background: #1d1d1f; border: 1px solid #2b2b2e; border-radius: 8px;
    color: #e8e8e8; font-size: 13px; padding: 6px; outline: none;
}
QListWidget::item { padding: 10px 12px; border-radius: 6px; }
QListWidget::item:hover { background: #26262a; }
QListWidget::item:selected { background: #2f3550; color: #ffffff; }
QTreeWidget {
    background: #1d1d1f; border: 1px solid #2b2b2e; border-radius: 8px;
    color: #e8e8e8; font-size: 13px; padding: 4px; outline: none;
}
QTreeWidget::item { padding: 6px 8px; border-radius: 6px; }
QTreeWidget::item:hover { background: #26262a; }
QTreeWidget::item:selected { background: #2f3550; color: #ffffff; }
QTreeWidget::branch { background: transparent; }
)"));

    // 左侧竖向导航（文字竖排）+ 右侧页面
    m_nav = new QListWidget(this);
    m_nav->setObjectName(QStringLiteral("nav"));
    m_nav->setFixedWidth(100);
    m_nav->setFocusPolicy(Qt::NoFocus);
    const QStringList navTexts = {QStringLiteral("⚙\n常\n规"),
                                  QStringLiteral("◉\n服\n务"),
                                  QStringLiteral("⇄\n更\n新"),
                                  QStringLiteral("ℹ\n关\n于")};
    const int navHeight = 92; // 各 tab 等高，视觉整齐
    for (int i = 0; i < navTexts.size(); ++i) {
        auto *item = new QListWidgetItem(navTexts[i], m_nav);
        item->setTextAlignment(Qt::AlignCenter);
        item->setSizeHint(QSize(82, navHeight));
    }
    connect(m_nav, &QListWidget::currentRowChanged, this, &SettingsDialog::onNavChanged);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(buildGeneralTab());
    m_pages->addWidget(buildServiceTab());
    m_pages->addWidget(buildRepoUpdateTab());
    m_pages->addWidget(buildAboutTab());

    // 必须在 m_pages 建好之后再设当前行（否则 currentRowChanged 触发时页面容器为空）
    m_nav->setCurrentRow(0);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 20);
    layout->setSpacing(14);
    layout->addWidget(m_nav);
    layout->addWidget(m_pages, 1);

    refreshRepo();
    refreshServiceUi();
    // 打开时反查一次：填充 PID / 源码路径
    m_proc->inspectAsync([this](const ServiceInfo &info) {
        if (info.ok) {
            m_svcPidLabel->setText(info.pid > 0 ? QStringLiteral("PID：%1").arg(info.pid)
                                                : QStringLiteral("PID：未知"));
            if (!info.sourceRoot.isEmpty())
                m_svcSourceLabel->setText(QStringLiteral("源码：%1").arg(info.sourceRoot));
        } else {
            m_svcPidLabel->setText(QStringLiteral("PID：无（端口无服务）"));
        }
    });
}

QWidget *SettingsDialog::buildServiceTab()
{
    auto *w = new QWidget(this);
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(24, 20, 24, 20);
    v->setSpacing(16);

    // 状态 + 要素卡片
    auto *card = new QFrame(w);
    card->setObjectName(QStringLiteral("card"));
    auto *cv = new QVBoxLayout(card);
    cv->setContentsMargins(22, 18, 22, 18);
    cv->setSpacing(12);

    m_svcStatusLabel = new QLabel(card);
    m_svcStatusLabel->setTextFormat(Qt::RichText);

    m_svcPidLabel = new QLabel(card);
    m_svcSourceLabel = new QLabel(card);
    m_svcSourceLabel->setWordWrap(true);
    auto *portLabel = new QLabel(QStringLiteral("端口：%1").arg(m_settings->webPort), card);
    auto *urlLabel = new QLabel(QStringLiteral("服务地址：http://127.0.0.1:%1").arg(m_settings->webPort), card);
    const QString logPath = QCoreApplication::applicationDirPath() + QStringLiteral("/config/dsh-web.log");
    auto *logLabel = new QLabel(QStringLiteral("日志：%1").arg(logPath), card);
    logLabel->setWordWrap(true);

    const QString labelStyle = QStringLiteral("color:#b5b5b5; font-size:13px;");
    const QList<QLabel *> labels = {m_svcPidLabel, m_svcSourceLabel, portLabel, urlLabel, logLabel};
    for (QLabel *l : labels)
        l->setStyleSheet(labelStyle);

    m_svcPidLabel->setText(QStringLiteral("PID：检测中..."));
    m_svcSourceLabel->setText(QStringLiteral("源码：%1").arg(m_settings->sourcePath));

    cv->addWidget(m_svcStatusLabel);
    cv->addSpacing(4);
    cv->addWidget(m_svcPidLabel);
    cv->addWidget(m_svcSourceLabel);
    cv->addWidget(portLabel);
    cv->addWidget(urlLabel);
    cv->addWidget(logLabel);

    // 控制按钮
    m_svcStartBtn = makeButton(QStringLiteral("启动"));
    m_svcStartBtn->setObjectName(QStringLiteral("primary"));
    m_svcStopBtn = makeButton(QStringLiteral("停止"));
    m_svcRestartBtn = makeButton(QStringLiteral("重启"));
    connect(m_svcStartBtn, &QPushButton::clicked, this, &SettingsDialog::onStartService);
    connect(m_svcStopBtn, &QPushButton::clicked, this, &SettingsDialog::onStopService);
    connect(m_svcRestartBtn, &QPushButton::clicked, this, &SettingsDialog::onRestartService);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(12);
    btnRow->addWidget(m_svcStartBtn);
    btnRow->addWidget(m_svcStopBtn);
    btnRow->addWidget(m_svcRestartBtn);
    btnRow->addStretch(1);

    v->addWidget(card);
    v->addLayout(btnRow);
    v->addStretch(1);
    return w;
}

void SettingsDialog::refreshServiceUi()
{
    using S = DshProcessManager::State;
    const S st = m_proc->state();
    QString color, text;
    switch (st) {
    case S::Idle:
        color = QStringLiteral("#787878");
        text = QStringLiteral("未运行");
        break;
    case S::Starting:
        color = QStringLiteral("#e0a030");
        text = QStringLiteral("启动中");
        break;
    case S::Running:
        color = QStringLiteral("#4caf50");
        text = QStringLiteral("运行中");
        break;
    case S::Stopping:
        color = QStringLiteral("#e07030");
        text = QStringLiteral("停止中");
        break;
    case S::Crashed:
        color = QStringLiteral("#e06060");
        text = QStringLiteral("异常");
        break;
    }
    m_svcStatusLabel->setText(QStringLiteral("<span style='color:%1; font-size:20px;'>●</span>"
                                             " <span style='color:#f5f5f5; font-size:16px; font-weight:700;'>%2</span>")
                                  .arg(color, text));

    const bool running = (st == S::Running);
    const bool transitioning = (st == S::Starting || st == S::Stopping);
    m_svcStartBtn->setEnabled(!running && !transitioning);
    m_svcStopBtn->setEnabled(running);
    m_svcRestartBtn->setEnabled(running);
}

void SettingsDialog::onStartService()
{
    m_proc->start(); // 异步清理端口残留后启动
}

void SettingsDialog::onStopService()
{
    m_proc->stop();
}

void SettingsDialog::onRestartService()
{
    m_proc->restart();
}

QWidget *SettingsDialog::buildGeneralTab()
{
    auto *w = new QWidget(this);
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(28, 24, 28, 24);
    v->setSpacing(18);

    m_sourcePathEdit = new QLineEdit(m_settings->sourcePath, w);
    m_portSpin = new QSpinBox(w);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(m_settings->webPort);
    m_nodePathEdit = new QLineEdit(m_settings->nodePath, w);
    m_nodePathEdit->setPlaceholderText(QStringLiteral("留空表示使用 PATH 中的 Node.js"));
    m_pnpmPathEdit = new QLineEdit(m_settings->pnpmPath, w);
    m_pnpmPathEdit->setPlaceholderText(QStringLiteral("留空表示使用 PATH 中的 pnpm"));
    m_gitPathEdit = new QLineEdit(m_settings->gitPath, w);
    m_gitPathEdit->setPlaceholderText(QStringLiteral("留空表示使用 PATH 中的 git"));
    m_repoUrlEdit = new QLineEdit(m_settings->repoUrl, w);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(14);
    grid->setColumnStretch(1, 1);
    int r = 0;
    grid->addWidget(fieldTitle(QStringLiteral("deepseek-harness 仓库路径"), w), r, 0);
    grid->addWidget(m_sourcePathEdit, r++, 1);
    grid->addWidget(fieldTitle(QStringLiteral("Web UI 端口"), w), r, 0);
    grid->addWidget(m_portSpin, r++, 1);
    grid->addWidget(fieldTitle(QStringLiteral("Node.js 路径"), w), r, 0);
    grid->addWidget(m_nodePathEdit, r++, 1);
    grid->addWidget(fieldTitle(QStringLiteral("pnpm 路径"), w), r, 0);
    grid->addWidget(m_pnpmPathEdit, r++, 1);
    grid->addWidget(fieldTitle(QStringLiteral("git 路径"), w), r, 0);
    grid->addWidget(m_gitPathEdit, r++, 1);
    grid->addWidget(fieldTitle(QStringLiteral("仓库地址"), w), r, 0);
    grid->addWidget(m_repoUrlEdit, r++, 1);

    auto *saveBtn = makeButton(QStringLiteral("保存配置"));
    saveBtn->setObjectName(QStringLiteral("primary"));
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::saveSettings);

    auto *row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(saveBtn);

    v->addLayout(grid);
    v->addStretch(1); // 保存按钮沉底
    v->addLayout(row);
    return w;
}

QWidget *SettingsDialog::buildRepoUpdateTab()
{
    auto *w = new QWidget(this);
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(24, 20, 24, 20);
    v->setSpacing(14);

    // —— 顶栏一行：当前分支 + 同步状态 + 操作按钮 ——
    m_branchLabel = new QLabel(w);
    m_branchLabel->setStyleSheet(QStringLiteral("color:#f5f5f5; font-size:15px; font-weight:700;"));
    m_statusLabel = new QLabel(w);
    m_statusLabel->setTextFormat(Qt::RichText);
    m_statusLabel->setStyleSheet(QStringLiteral("font-size:13px;"));

    auto *fetchBtn = makeButton(QStringLiteral("Fetch 刷新"));
    auto *updateBtn = makeButton(QStringLiteral("更新当前分支"));
    connect(fetchBtn, &QPushButton::clicked, this, &SettingsDialog::onFetch);
    connect(updateBtn, &QPushButton::clicked, this, &SettingsDialog::onUpdateCurrentBranch);

    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(12);
    topRow->addWidget(m_branchLabel);
    topRow->addWidget(m_statusLabel);
    topRow->addStretch(1);
    topRow->addWidget(fetchBtn);
    topRow->addWidget(updateBtn);
    v->addLayout(topRow);

    // —— 左：分支树 ——
    auto *branchCard = new QFrame(w);
    branchCard->setObjectName(QStringLiteral("card"));
    branchCard->setFixedWidth(250);
    auto *bc = new QVBoxLayout(branchCard);
    bc->setContentsMargins(16, 14, 16, 14);
    bc->setSpacing(10);
    auto *branchTitle = new QLabel(QStringLiteral("分支"), branchCard);
    branchTitle->setObjectName(QStringLiteral("pageTitle"));
    auto *switchBranchBtn = makeButton(QStringLiteral("切换到该分支"));
    connect(switchBranchBtn, &QPushButton::clicked, this, &SettingsDialog::onSwitchBranch);
    auto *branchTitleRow = new QHBoxLayout;
    branchTitleRow->addWidget(branchTitle);
    branchTitleRow->addStretch(1);
    branchTitleRow->addWidget(switchBranchBtn);
    m_branchTree = new QTreeWidget(branchCard);
    m_branchTree->setHeaderHidden(true);
    m_branchTree->setFocusPolicy(Qt::NoFocus);
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
    auto *commitCard = new QFrame(w);
    commitCard->setObjectName(QStringLiteral("card"));
    auto *cc = new QVBoxLayout(commitCard);
    cc->setContentsMargins(16, 14, 16, 14);
    cc->setSpacing(10);
    auto *commitTitle = new QLabel(QStringLiteral("提交记录（双击行切换到此提交）"), commitCard);
    commitTitle->setObjectName(QStringLiteral("pageTitle"));
    auto *switchCommitBtn = makeButton(QStringLiteral("切换到该提交"));
    connect(switchCommitBtn, &QPushButton::clicked, this, &SettingsDialog::onSwitchCommitSelected);
    auto *commitTitleRow = new QHBoxLayout;
    commitTitleRow->addWidget(commitTitle);
    commitTitleRow->addStretch(1);
    commitTitleRow->addWidget(switchCommitBtn);
    m_commitList = new QListWidget(commitCard);
    m_commitList->setFocusPolicy(Qt::NoFocus);
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
    return w;
}

QWidget *SettingsDialog::buildAboutTab()
{
    auto *w = new QWidget(this);
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(28, 24, 28, 24);
    v->setSpacing(14);

    auto *name = new QLabel(QStringLiteral("deepseek-harness-in-qt"), w);
    name->setStyleSheet(QStringLiteral("color:#f5f5f5; font-size:20px; font-weight:700;"));

    // 软件定位
    auto *posTitle = new QLabel(QStringLiteral("软件定位"), w);
    posTitle->setObjectName(QStringLiteral("pageTitle"));
    auto *pos = new QLabel(QStringLiteral("deepseek-harness-in-qt 是 deepseek-harness（dsh）的桌面化管理工具。"
                                          "它把 dsh 从命令行变成常驻后台服务：用图形界面完成环境检测、更新"
                                          "与服务管理，内置 Web 界面，无需记忆任何命令。"),
                           w);
    pos->setWordWrap(true);
    pos->setStyleSheet(QStringLiteral("color:#c8c8c8; font-size:13px;"));

    // 主要特性
    auto *featTitle = new QLabel(QStringLiteral("主要特性"), w);
    featTitle->setObjectName(QStringLiteral("pageTitle"));
    auto *feats = new QLabel(QStringLiteral("• 环境自动检测：Node.js / pnpm / git 一键校验\n"
                                            "• 更新一目了然：与上游的领先 / 落后状态清晰可见\n"
                                            "• 更新管理：切换分支、切换提交、更新当前分支\n"
                                            "• 服务常驻后台：关闭窗口不影响 dsh 继续运行\n"
                                            "• 内置 Web UI：以浏览器内核渲染 dsh 界面"),
                             w);
    feats->setWordWrap(true);
    feats->setStyleSheet(QStringLiteral("color:#9a9a9a; font-size:13px;"));

    auto *repo = new QLabel(QStringLiteral("上游仓库：https://github.com/deepseek-ai/deepseek-harness"), w);
    repo->setStyleSheet(QStringLiteral("color:#7ab0ff; font-size:12px;"));
    auto *hint = new QLabel(QStringLiteral("dsh 作为常驻后台服务运行，关闭本窗口不影响其继续运行。"), w);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#787878; font-size:12px;"));

    v->addWidget(name);
    v->addSpacing(6);
    v->addWidget(posTitle);
    v->addWidget(pos);
    v->addSpacing(8);
    v->addWidget(featTitle);
    v->addWidget(feats);
    v->addStretch(1);
    v->addWidget(repo);
    v->addWidget(hint);
    return w;
}

QPushButton *SettingsDialog::makeButton(const QString &text)
{
    auto *b = new QPushButton(text, this);
    b->setObjectName(QStringLiteral("secondary"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

void SettingsDialog::saveSettings()
{
    const QString path = m_sourcePathEdit->text().trimmed();
    if (!QFileInfo::exists(path + QStringLiteral("/pnpm-workspace.yaml"))) {
        QMessageBox::warning(this, QStringLiteral("设置"), QStringLiteral("源码路径无效：未找到 pnpm-workspace.yaml"));
        return;
    }
    m_settings->sourcePath = path;
    m_settings->webPort = m_portSpin->value();
    m_settings->nodePath = m_nodePathEdit->text().trimmed();
    m_settings->pnpmPath = m_pnpmPathEdit->text().trimmed();
    m_settings->gitPath = m_gitPathEdit->text().trimmed();
    m_settings->repoUrl = m_repoUrlEdit->text().trimmed();
    m_settings->save();
    refreshRepo();
    QMessageBox::information(this, QStringLiteral("设置"), QStringLiteral("配置已保存。"));
}

void SettingsDialog::refreshRepo()
{
    if (m_watcher->isRunning())
        return; // 上一次刷新仍在后台进行
    m_branchLabel->setText(QStringLiteral("…"));
    m_statusLabel->setText(QStringLiteral("<span style='color:#9a9a9a;'>● 正在刷新仓库信息...</span>"));
    m_branchTree->clear();
    m_commitList->clear();
    m_watcher->setFuture(QtConcurrent::run([this] { return collectSnapshot(); }));
}

RepoSnapshot SettingsDialog::collectSnapshot()
{
    RepoSnapshot s;
    s.branch = m_git->currentBranch();
    int ahead = 0, behind = 0;
    s.aheadValid = m_git->aheadBehind(&ahead, &behind);
    s.ahead = ahead;
    s.behind = behind;
    s.dirty = m_git->isDirty();
    s.branches = m_git->branches();
    s.commits = m_git->commits(60, 0);
    return s;
}

void SettingsDialog::onRepoSnapshotReady()
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

void SettingsDialog::populateCommits()
{
    m_commitList->clear();
    for (const auto &c : m_commits) {
        m_commitList->addItem(QStringLiteral("● %1  %2    %3 · %4").arg(c.hash.left(7), c.message, c.author, c.date));
    }
}

void SettingsDialog::populateBranchTree(const QList<GitBranch> &branches)
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

void SettingsDialog::loadBranchCommits(const QString &rev)
{
    if (m_commitWatcher->isRunning())
        return;
    m_commitList->clear();
    m_commitList->addItem(QStringLiteral("加载中..."));
    m_commitWatcher->setFuture(QtConcurrent::run([this, rev] { return m_git->commits(rev, 60, 0); }));
}

void SettingsDialog::onBranchTreeChanged()
{
    QTreeWidgetItem *item = m_branchTree->currentItem();
    if (!item)
        return;
    const QString name = item->data(0, Qt::UserRole).toString();
    if (name.isEmpty())
        return; // 组节点
    loadBranchCommits(name);
}

void SettingsDialog::onBranchCommitsReady()
{
    m_commits = m_commitWatcher->result();
    populateCommits();
}

void SettingsDialog::onFetch()
{
    QString err;
    if (!m_git->fetch(&err)) {
        m_statusLabel->setText(
            QStringLiteral("<span style='color:#e06060;'>● Fetch 失败：%1</span>").arg(err.toHtmlEscaped()));
        return;
    }
    refreshRepo();
}

void SettingsDialog::onCommitActivated(int row)
{
    if (row < 0 || row >= m_commits.size())
        return;
    const QString hash = m_commits[row].hash;
    UpdateManager::Target t;
    t.kind = UpdateManager::Target::Commit;
    t.value = hash;
    m_update->start(t); // 更新在后台执行，对话框保留
}

void SettingsDialog::onUpdateCurrentBranch()
{
    const QString branch = m_git->currentBranch();
    if (branch.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("更新"), QStringLiteral("无法获取当前分支"));
        return;
    }
    UpdateManager::Target t;
    t.kind = UpdateManager::Target::Branch;
    t.value = branch;
    m_update->start(t); // 更新在后台执行，对话框保留
}

void SettingsDialog::onSwitchBranch()
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
    m_update->start(t); // 切换在后台执行，对话框保留
}

void SettingsDialog::onSwitchCommitSelected()
{
    onCommitActivated(m_commitList->currentRow());
}

void SettingsDialog::onNavChanged(int row)
{
    if (!m_pages)
        return; // 页面容器尚未创建（防御）
    if (row >= 0 && row < m_pages->count())
        m_pages->setCurrentIndex(row);
}
