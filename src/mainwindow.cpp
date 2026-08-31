#include "mainwindow.h"

#include <QCloseEvent>
#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>

#include "git/gitclient.h"
#include "ui/theme.h"
#include "process/environmentchecker.h"
#include "process/preflightchecker.h"
#include "settings/settingsdialog.h"
#include "ui/errorpage.h"
#include "ui/homepage.h"
#include "ui/logview.h"
#include "ui/setuppage.h"
#include "ui/startuppage.h"
#include "update/updatemanager.h"

namespace dshinqt {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("deepseek-harness-in-qt"));
    qDebug() << "[UI] === MainWindow 构造开始 ===";

    // 窗口级背景色（深色，与 dsh 主题一致），避免 widget 层露出浅色
    const QColor windowBg(18, 18, 18);
    setAutoFillBackground(true);
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, windowBg);
        pal.setColor(QPalette::Base, windowBg);
        pal.setColor(QPalette::WindowText, QColor(220, 220, 220));
        pal.setColor(QPalette::Text, QColor(220, 220, 220));
        pal.setColor(QPalette::Button, QColor(38, 38, 42));
        pal.setColor(QPalette::ButtonText, QColor(220, 220, 220));
        setPalette(pal);
    }

    m_settings.load();

    // 先自动探测环境路径（node/pnpm/git），引导页 prefill 时即可显示
    m_env = new EnvironmentChecker(this);
    m_env->autoDetect(&m_settings);
    qDebug() << "[UI] autoDetect 完成 sourcePath=" << m_settings.sourcePath << "node=" << m_settings.nodePath;

    // 中央叠层容器：stackwidget（普通页面）与 webview 平级、同几何叠放。
    // webview 常驻渲染并默认 lower（被 stackwidget 盖住），切页用 raise/lower，
    // 避免 hide/show 导致的重渲染与白闪。
    auto *central = new QWidget(this);
    central->setAutoFillBackground(true);
    central->setPalette(palette());
    auto *lay = new QGridLayout(central);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_pages = new QStackedWidget(central);
    m_pages->setAutoFillBackground(true);
    m_pages->setPalette(palette());
    lay->addWidget(m_pages, 0, 0);

    // 启动加载页（首帧：显示检测/启动服务状态）
    m_startupPage = new StartupPage(m_pages);
    m_pages->addWidget(m_startupPage);

    // 引导页（环境缺失时补全路径）
    m_setupPage = new SetupPage(&m_settings, m_env, m_pages);
    m_pages->addWidget(m_setupPage);

    // 错误页（启动受阻时展示原因与操作入口）
    m_errorPage = new ErrorPage(m_pages);
    m_pages->addWidget(m_errorPage);

    m_logView = new LogView(m_pages);
    m_pages->addWidget(m_logView);

    // 主页（webview）：与 stackwidget 平级叠放，常驻渲染
    m_homePage = new HomePage(&m_settings, central);
    lay->addWidget(m_homePage, 0, 0);
    m_homePage->lower();
    // 页面下载事件（开始/完成/失败）转发到日志页
    connect(m_homePage, &HomePage::downloadLog, this, &MainWindow::onDshLog);

    setCentralWidget(central);

    m_process = new DshProcessManager(&m_settings, this);
    m_preflight = new PreflightChecker(this);
    m_git = new GitClient(&m_settings, this);
    m_update = new UpdateManager(&m_settings, m_git, m_process, this);
    m_buildFlow = new BuildFlowManager(&m_settings, this);
    m_staleWatcher = new QFutureWatcher<BuildStalenessInfo>(this);
    connect(m_staleWatcher, &QFutureWatcher<BuildStalenessInfo>::finished, this,
            &MainWindow::onStaleCheckFinished);

    connect(m_process, &DshProcessManager::stateChanged, this, &MainWindow::onDshStateChanged);
    connect(m_process, &DshProcessManager::logOutput, this, &MainWindow::onDshLog);
    connect(m_buildFlow, &BuildFlowManager::logOutput, this, &MainWindow::onDshLog);
    connect(m_buildFlow, &BuildFlowManager::buildFinished, this, &MainWindow::onBuildFinished);
    // 主页就绪后再显示：避免用户看到 dsh 早期加载的 “failed to load plugins” 警告
    connect(m_homePage, &HomePage::pageReady, this, [this]() {
        qDebug() << "[UI] HomePage 就绪 -> 显示主页";
        showHomePage();
    });
    connect(m_homePage, &HomePage::pageFailed, this, [this]() {
        showErrorPage(QStringLiteral("deepseek-harness 界面未在预期时间内就绪，请查看日志页确认。"),
                      /*canBuild=*/false);
    });
    connect(m_setupPage, &SetupPage::finished, this, &MainWindow::onSetupFinished);
    connect(m_setupPage, &SetupPage::checkFailed, this, &MainWindow::showSetupPage);
    connect(m_setupPage, &SetupPage::buildRequested, this,
            [this]() { runOneClickBuild(BuildFlowManager::Origin::SetupPage); });
    connect(m_setupPage, &SetupPage::cloneRequested, this, &MainWindow::startClone);
    connect(m_errorPage, &ErrorPage::retryRequested, this, &MainWindow::onErrorRetry);
    connect(m_errorPage, &ErrorPage::buildRequested, this, &MainWindow::onErrorBuild);
    connect(m_errorPage, &ErrorPage::openLogRequested, this, &MainWindow::showLogPage);
    connect(m_logView, &LogView::backRequested, this, &MainWindow::showHomePage);
    connect(m_update, &UpdateManager::logOutput, this, &MainWindow::onDshLog);
    connect(m_update, &UpdateManager::stageChanged, this, &MainWindow::onUpdateStageChanged);
    connect(m_update, &UpdateManager::finished, this, &MainWindow::onUpdateFinished);

    // 状态栏：左侧图标按钮（设置/日志/刷新）+ 右侧状态消息 label。
    // 状态用 QLabel 而非 showMessage——showMessage 会隐藏 addWidget 的按钮。
    // 图标按钮统一：深色底 + 圆角，图标灰（Normal）/ 蓝（Active 即 hover）双色。
    const QString iconBtnQss = QStringLiteral(R"(
QPushButton {
    background: #2c2c31; border: 1px solid #38383e; border-radius: 8px;
    margin-left: 6px;
}
QPushButton:hover { background: #35353b; border-color: #4f8cff; }
QPushButton:pressed { background: #2f3550; }
)");
    const auto makeIconBtn = [this, iconBtnQss](const QString &normalIcon, const QString &activeIcon,
                                                 const QString &tooltip) {
        auto *btn = new QPushButton(this);
        btn->setFixedSize(22, 18);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::TabFocus);
        btn->setToolTip(tooltip);
        btn->setStyleSheet(iconBtnQss);
        btn->setIconSize(QSize(13, 13));
        QIcon icon;
        icon.addFile(normalIcon, QSize(), QIcon::Normal, QIcon::Off);
        icon.addFile(activeIcon, QSize(), QIcon::Active, QIcon::Off);
        btn->setIcon(icon);
        return btn;
    };

    auto *settingsBtn = makeIconBtn(QStringLiteral(":/icons/settings.svg"), QStringLiteral(":/icons/settings-active.svg"),
                                    QStringLiteral("打开设置"));
    connect(settingsBtn, &QPushButton::clicked, this, [this] { openSettings(); });
    statusBar()->addWidget(settingsBtn); // 左侧

    auto *logBtn = makeIconBtn(QStringLiteral(":/icons/log.svg"), QStringLiteral(":/icons/log-active.svg"),
                               QStringLiteral("打开日志页"));
    connect(logBtn, &QPushButton::clicked, this, &MainWindow::showLogPage);
    statusBar()->addWidget(logBtn);

    // 快速修复：直达设置弹窗「修复」页（会话日志损坏时少两步）
    auto *repairBtn = makeIconBtn(QStringLiteral(":/icons/wrench.svg"), QStringLiteral(":/icons/wrench-active.svg"),
                                  QStringLiteral("会话快速修复"));
    connect(repairBtn, &QPushButton::clicked, this, [this] { openSettings(/*修复页*/ 3); });
    statusBar()->addWidget(repairBtn);

    // 开放平台：一键外开 DeepSeek 控制台（用量/余额/充值/API Key 管理均需登录网页）
    auto *platformBtn = makeIconBtn(QStringLiteral(":/icons/external.svg"), QStringLiteral(":/icons/external-active.svg"),
                                    QStringLiteral("打开 DeepSeek 开放平台"));
    connect(platformBtn, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://platform.deepseek.com")));
    });
    statusBar()->addWidget(platformBtn);

    auto *reloadBtn = makeIconBtn(QStringLiteral(":/icons/refresh.svg"), QStringLiteral(":/icons/refresh-active.svg"),
                                  QStringLiteral("刷新 dsh Web UI（F5）"));
    connect(reloadBtn, &QPushButton::clicked, this, [this] {
        m_homePage->load(QUrl(m_process->webUrl())); // 带认证 token（dsh web token 认证）
        showHomePage();
    });
    statusBar()->addWidget(reloadBtn);

    auto *reloadShortcut = new QShortcut(QKeySequence::Refresh, this); // F5
    connect(reloadShortcut, &QShortcut::activated, reloadBtn, &QPushButton::click);

    m_statusLabel = new QLabel(QStringLiteral("dsh: 未启动"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#9a9a9a; font-size:12px;"));
    statusBar()->addWidget(m_statusLabel); // 消息区（按钮右侧），不被挤到最右

    // 首帧显示启动加载页（而非引导页），随后自动校验环境：
    // 齐全 → 启动服务 → Running 切主页；缺失 → 切引导页补全。
    qDebug() << "[UI] 首帧 -> StartupPage";
    m_pages->setCurrentWidget(m_startupPage);
    QTimer::singleShot(0, this, [this]() {
        qDebug() << "[UI] 触发 autoCheck（自动校验）";
        m_setupPage->autoCheck();
    });

    resize(1280, 800);
    setMinimumSize(980, 680); // 保证引导页卡片（最小宽约 716px）等页面内容不被裁切
    qDebug() << "[UI] === MainWindow 构造完成 ===";
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 常驻服务模式：退出不杀 dsh，只释放 webview
    m_homePage->shutdown();
    event->accept();
}

void MainWindow::showHomePage()
{
    qDebug() << "[UI] showHomePage (raise webview)";
    m_homePage->raise(); // webview 盖住 stackwidget
}

void MainWindow::showLogPage()
{
    qDebug() << "[UI] showLogPage";
    m_homePage->lower();
    m_pages->setCurrentWidget(m_logView);
}

void MainWindow::openSettings(int page)
{
    SettingsDialog dlg(&m_settings, m_git, m_update, m_process, this, page);
    dlg.exec();
}

void MainWindow::onUpdateStageChanged(UpdateManager::Stage stage)
{
    QString text;
    switch (stage) {
    case UpdateManager::Stage::Stopping: text = QStringLiteral("停止 dsh"); break;
    case UpdateManager::Stage::Fetch: text = QStringLiteral("拉取"); break;
    case UpdateManager::Stage::Checkout: text = QStringLiteral("切换"); break;
    case UpdateManager::Stage::Pull: text = QStringLiteral("合并更新"); break;
    case UpdateManager::Stage::Install: text = QStringLiteral("装依赖"); break;
    case UpdateManager::Stage::Build: text = QStringLiteral("构建"); break;
    case UpdateManager::Stage::Starting: text = QStringLiteral("启动"); break;
    case UpdateManager::Stage::Done: text = QStringLiteral("完成"); break;
    case UpdateManager::Stage::Failed: text = QStringLiteral("失败"); break;
    case UpdateManager::Stage::Idle: return;
    }
    m_statusLabel->setText(QStringLiteral("更新：%1").arg(text));
}

void MainWindow::onUpdateFinished(bool success, const QString &error)
{
    if (success) {
        m_statusLabel->setText(QStringLiteral("更新完成"));
    } else {
        QMessageBox::warning(this, QStringLiteral("更新失败"), error);
        showLogPage();
    }
    // 恢复为 dsh 运行状态文案（更新阶段文字只在 onUpdateStageChanged 中短暂覆盖）
    onDshStateChanged(m_process->state());
}

void MainWindow::onSetupFinished()
{
    qDebug() << "[UI] onSetupFinished 校验通过，启动服务（等 Running 再切主页）";
    m_settings.save();
    m_statusLabel->setText(QStringLiteral("环境配置完成"));
    // 保持加载页，更新状态文字；等 dsh Running 由 onDshStateChanged 切主页。
    m_startupPage->setStatus(QStringLiteral("正在启动服务..."));
    continueToService();
}

void MainWindow::continueToService()
{
    qDebug() << "[UI] continueToService";
    // 启动前自检：外部 git pull 后构建产物过期时提示重建，避免"跑着旧构建"导致页面异常
    checkStaleBuildAndProceed();
}

// 启动前构建产物自检（异步，不阻塞 UI）：
// 后台线程取 HEAD 提交时间 + dist/index.html 修改时间；过期则在主线程弹确认框。
// 判据见 buildstaleness.h；git 不可用/非仓库/产物新鲜 → 直接走原启动流程。
void MainWindow::checkStaleBuildAndProceed()
{
    if (m_staleWatcher->isRunning())
        return; // 检查在途，忽略重复触发
    if (!EnvironmentChecker::isSourceValid(m_settings.sourcePath)) {
        m_process->ensureRunning(); // 源码路径无效：原流程（体检/引导页兜底）
        return;
    }
    const QString src = m_settings.sourcePath;
    GitClient *git = m_git;
    m_staleWatcher->setFuture(QtConcurrent::run([git, src]() {
        BuildStalenessInfo info;
        info.distMtimeMs = distIndexMtimeMs(src);
        git->headCommit(&info.hash7, &info.commitEpochSec, src);
        return info;
    }));
}

void MainWindow::onStaleCheckFinished()
{
    const BuildStalenessInfo info = m_staleWatcher->result();
    if (info.commitEpochSec < 0) {
        m_process->ensureRunning(); // 非 git 仓库/取不到提交 → 无法判定，原流程
        return;
    }
    if (!isDistStale(info.commitEpochSec, info.distMtimeMs)) {
        m_process->ensureRunning(); // 产物新鲜（含外部手工构建过）
        return;
    }

    const QString date = QDateTime::fromSecsSinceEpoch(info.commitEpochSec)
                             .toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    const auto choice = QMessageBox::question(
        this, QStringLiteral("检测到源码更新"),
        QStringLiteral("仓库已更新到 %1（提交于 %2），当前构建产物（apps/web/dist）早于该提交。\n"
                       "不重建直接启动，页面可能因产物过期而异常。\n\n是否立即重新构建？")
            .arg(info.hash7, date),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (choice == QMessageBox::Yes) {
        qDebug() << "[UI] 构建产物过期，用户确认重建";
        // 立即给出可见反馈：pnpm 首行输出有 1~2s 启动延迟，
        // 若日志页空白会让人误以为程序卡死
        m_logView->appendLog(
            QStringLiteral("[构建] 检测到构建产物过期（HEAD %1 提交于 %2），开始重新构建...").arg(info.hash7, date));
        if (m_process->isRunning()) {
            m_logView->appendLog(QStringLiteral("[构建] 正在停止旧服务..."));
            m_process->stop(); // 旧服务（旧产物）先停；构建完成后 startService 重新拉起
        }
        showLogPage(); // 构建进度走 CLI 视口
        m_buildFlow->startOneClickBuild(BuildFlowManager::Origin::Startup);
        return;
    }
    qDebug() << "[UI] 用户选择直接启动（产物可能过期）";
    m_process->ensureRunning();
}

void MainWindow::startService()
{
    const auto items = m_preflight->check(m_settings);
    QStringList failed;
    for (const auto &it : items) {
        m_logView->appendLog(QStringLiteral("[体检] %1：%2（%3）")
                                 .arg(it.name, it.passed ? QStringLiteral("OK") : QStringLiteral("FAIL"), it.detail),
                             !it.passed);
        if (!it.passed)
            failed << QStringLiteral("%1：%2").arg(it.name, it.detail);
    }

    if (!failed.isEmpty()) {
        showErrorPage(
            QStringLiteral("缺少依赖或构建产物：\n\n%1\n\n请先一键构建。").arg(failed.join(QLatin1Char('\n'))),
            /*canBuild=*/true);
        return;
    }

    m_process->start();
}

void MainWindow::showSetupPage()
{
    qDebug() << "[UI] showSetupPage";
    m_homePage->lower();
    m_pages->setCurrentWidget(m_setupPage);
}

void MainWindow::showErrorPage(const QString &message, bool canBuild)
{
    qDebug() << "[UI] showErrorPage canBuild=" << canBuild << "msg=" << message;
    m_errorPage->setMessage(message);
    m_errorPage->setBuildVisible(canBuild);
    m_homePage->lower();
    m_pages->setCurrentWidget(m_errorPage);
}

void MainWindow::onErrorRetry()
{
    if (m_process->isRunning()) {
        // 服务仍在运行，只是页面加载失败 → 页面级重试（load 会重置超时与自愈标记）
        m_homePage->load(QUrl(m_process->webUrl()));
        showHomePage();
    } else {
        startService();
    }
}

void MainWindow::onErrorBuild()
{
    runOneClickBuild(BuildFlowManager::Origin::ErrorPage);
}

void MainWindow::runOneClickBuild(BuildFlowManager::Origin origin)
{
    showLogPage(); // 切到 CLI 视口，完整呈现 pnpm 输出
    // 先给即时反馈：pnpm 首行输出有启动延迟，空白会让人误以为卡死
    m_logView->appendLog(QStringLiteral("[构建] 开始构建：pnpm install && pnpm run build"));
    m_buildFlow->startOneClickBuild(origin);
}

void MainWindow::startClone()
{
    showLogPage(); // 切到 CLI 视口，完整呈现 git 输出（含 Receiving objects 进度）
    m_logView->appendLog(QStringLiteral("[构建] 开始克隆仓库：%1").arg(m_settings.repoUrl));
    m_buildFlow->startClone();
}

void MainWindow::onBuildFinished(bool success, const QString &error, BuildFlowManager::Origin origin)
{
    if (!success) {
        if (origin == BuildFlowManager::Origin::SetupPage) {
            QMessageBox::warning(this, QStringLiteral("构建失败"), error);
            m_setupPage->recheck();
            showSetupPage();
        } else if (origin == BuildFlowManager::Origin::Startup) {
            // 启动前重建失败：回错误页（可重试构建），日志页已展示失败细节
            showErrorPage(QStringLiteral("重新构建失败，请查看日志页确认原因。"), /*canBuild=*/true);
        }
        // 错误页来源：仅日志记录（原行为），用户可回错误页重试
        return;
    }
    if (origin == BuildFlowManager::Origin::SetupPage) {
        // 构建来自引导页：回到引导页重新校验
        m_setupPage->recheck();
        showSetupPage();
    } else {
        // ErrorPage / Startup：构建成功后直接启动服务（先给收尾反馈）
        m_logView->appendLog(QStringLiteral("[构建] 构建完成，正在启动服务..."));
        startService();
    }
}

void MainWindow::onDshStateChanged(DshProcessManager::State state)
{
    qDebug() << "[UI] onDshStateChanged ->" << DshProcessManager::stateName(state);
    QString text;
    switch (state) {
    case DshProcessManager::State::Idle: text = QStringLiteral("未启动"); break;
    case DshProcessManager::State::Starting: text = QStringLiteral("启动中"); break;
    case DshProcessManager::State::Running:
        text = QStringLiteral("运行中");
        qDebug() << "[UI] Running -> 后台加载 webview（锚点就绪后再显示）port=" << m_settings.webPort;
        m_homePage->load(QUrl(m_process->webUrl())); // 带认证 token：首次访问换取持久 cookie
        // 不在此处 showHomePage()：等 HomePage::pageReady（输入区/会话区已挂载）再 raise 显示，
        // 避免用户看到 dsh 早期加载的 “failed to load plugins” 警告。
        break;
    case DshProcessManager::State::Stopping: text = QStringLiteral("停止中"); break;
    case DshProcessManager::State::Crashed:
        text = QStringLiteral("启动失败");
        showErrorPage(QStringLiteral("deepseek-harness 启动失败，详见日志页。"), /*canBuild=*/false);
        break;
    }
    m_statusLabel->setText(QStringLiteral("dsh: %1").arg(text));
}

void MainWindow::onDshLog(const QString &line, bool isError)
{
    m_logView->appendLog(line, isError);
}

} // namespace dshinqt
