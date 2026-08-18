#include "mainwindow.h"

#include <QCloseEvent>
#include <QColor>
#include <QDebug>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>

#include "git/gitclient.h"
#include "process/environmentchecker.h"
#include "process/preflightchecker.h"
#include "process/proxydetector.h"
#include "process/startwrapped.h"
#include "settings/settingsdialog.h"
#include "ui/errorpage.h"
#include "ui/homepage.h"
#include "ui/logview.h"
#include "ui/setuppage.h"
#include "ui/startuppage.h"
#include "update/updatemanager.h"

namespace dshinqt {

namespace {
const char *stateName(DshProcessManager::State s)
{
    switch (s) {
    case DshProcessManager::State::Idle: return "Idle";
    case DshProcessManager::State::Starting: return "Starting";
    case DshProcessManager::State::Running: return "Running";
    case DshProcessManager::State::Stopping: return "Stopping";
    case DshProcessManager::State::Crashed: return "Crashed";
    }
    return "?";
}
} // namespace

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
    m_homePage = new HomePage(central);
    lay->addWidget(m_homePage, 0, 0);
    m_homePage->lower();

    setCentralWidget(central);

    m_process = new DshProcessManager(&m_settings, this);
    m_preflight = new PreflightChecker(this);
    m_git = new GitClient(&m_settings, this);
    m_update = new UpdateManager(&m_settings, m_git, m_process, this);

    connect(m_process, &DshProcessManager::stateChanged, this, &MainWindow::onDshStateChanged);
    connect(m_process, &DshProcessManager::logOutput, this, &MainWindow::onDshLog);
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
    connect(m_setupPage, &SetupPage::buildRequested, this, [this]() { runOneClickBuild(/*fromSetup=*/true); });
    connect(m_setupPage, &SetupPage::cloneRequested, this, &MainWindow::startClone);
    connect(m_errorPage, &ErrorPage::retryRequested, this, &MainWindow::onErrorRetry);
    connect(m_errorPage, &ErrorPage::buildRequested, this, &MainWindow::onErrorBuild);
    connect(m_errorPage, &ErrorPage::openLogRequested, this, &MainWindow::showLogPage);
    connect(m_update, &UpdateManager::logOutput, this, &MainWindow::onDshLog);
    connect(m_update, &UpdateManager::stageChanged, this, &MainWindow::onUpdateStageChanged);
    connect(m_update, &UpdateManager::finished, this, &MainWindow::onUpdateFinished);

    // 状态栏：左侧圆点设置按钮 + 右侧状态消息 label。
    // 状态用 QLabel 而非 showMessage——showMessage 会隐藏 addWidget 的按钮。
    auto *settingsBtn = new QPushButton(QStringLiteral(">"), this);
    settingsBtn->setFixedSize(16, 16);
    settingsBtn->setCursor(Qt::PointingHandCursor);
    settingsBtn->setFocusPolicy(Qt::NoFocus);
    settingsBtn->setToolTip(QStringLiteral("打开设置"));
    // 配色与全局深色主题一致
    settingsBtn->setStyleSheet(QStringLiteral(R"(
QPushButton {
    background: #2c2c31; border: 1px solid #4f8cff; border-radius: 8px;
    color: #4f8cff; font-size: 10px; font-weight: 700;
}
QPushButton:hover { background: #35353b; color: #7ab0ff; border-color: #7ab0ff; }
QPushButton:pressed { background: #2f3550; }
)"));
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettings);
    statusBar()->addWidget(settingsBtn); // 左侧

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

void MainWindow::openSettings()
{
    SettingsDialog dlg(&m_settings, m_git, m_update, m_process, this);
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
    // 服务在且匹配 → attach；不匹配 → 杀旧启新；不在 → 启动
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
    startService();
}

void MainWindow::onErrorBuild()
{
    runOneClickBuild(/*fromSetup=*/false);
}

void MainWindow::runOneClickBuild(bool fromSetup)
{
    m_buildFromSetup = fromSetup;
    showLogPage(); // 切到 CLI 视口，完整呈现 pnpm 输出
    runBuildStep({QStringLiteral("install")});
}

void MainWindow::startClone()
{
    const QString url = m_settings.repoUrl.isEmpty()
                            ? QStringLiteral("https://github.com/deepseek-ai/deepseek-harness.git")
                            : m_settings.repoUrl;
    const QString target = m_settings.sourcePath;
    const QString git = m_settings.gitProgram();

    // 切到 CLI 视口，完整呈现 git 输出（含 Receiving objects 进度）
    showLogPage();

    auto *proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(proc, &QProcess::readyReadStandardOutput, this, [proc, this]() {
        m_logView->appendLog(QString::fromUtf8(proc->readAllStandardOutput()));
    });
    connect(proc, &QProcess::finished, this, [proc, this](int code, QProcess::ExitStatus) {
        proc->deleteLater();
        if (code != 0) {
            m_logView->appendLog(QStringLiteral("git clone 失败（code=%1）").arg(code), true);
            QMessageBox::warning(this,
                                 QStringLiteral("克隆失败"),
                                 QStringLiteral("git clone 失败，请查看日志页确认原因（网络/仓库地址）。"));
            m_setupPage->recheck();
            showSetupPage();
            return;
        }
        m_logView->appendLog(QStringLiteral("克隆完成，开始安装依赖..."), false);
        m_buildFromSetup = true;
        runBuildStep({QStringLiteral("install")});
    });
    // FailedToStart 时 finished 不会触发，单独兜底
    connect(proc, &QProcess::errorOccurred, this, [proc, this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            proc->deleteLater();
            m_logView->appendLog(QStringLiteral("git 启动失败（gitPath 无效或权限不足）"), true);
            QMessageBox::warning(this, QStringLiteral("克隆失败"), QStringLiteral("git 启动失败，请检查 git 路径设置。"));
            m_setupPage->recheck();
            showSetupPage();
        }
    });

    QStringList gitArgs = ProxyDetector::gitProxyArgs();
    gitArgs << QStringLiteral("clone") << QStringLiteral("--progress") << url << target;
    m_logView->appendLog(QStringLiteral("> git clone %1 %2").arg(url, target));
    proc->start(git, gitArgs);
}

void MainWindow::runBuildStep(const QStringList &pnpmArgs)
{
    const QString pnpm = m_settings.pnpmPath.isEmpty() ? QStringLiteral("pnpm") : m_settings.pnpmPath;

    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(m_settings.sourcePath);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    // pnpm 不读 Windows 系统代理，自动注入代理环境变量，使 npm 包下载走代理
    proc->setProcessEnvironment(ProxyDetector::pnpmEnvironment());

    connect(proc, &QProcess::readyReadStandardOutput, this, [proc, this]() {
        m_logView->appendLog(QString::fromUtf8(proc->readAllStandardOutput()));
    });
    connect(proc, &QProcess::finished, this, [proc, pnpmArgs, this](int code, QProcess::ExitStatus) {
        proc->deleteLater();
        if (code != 0) {
            m_logView->appendLog(QStringLiteral("命令失败：pnpm %1").arg(pnpmArgs.join(' ')), true);
            if (m_buildFromSetup) {
                QMessageBox::warning(this,
                                     QStringLiteral("构建失败"),
                                     QStringLiteral("pnpm %1 失败，请查看日志页确认原因。").arg(pnpmArgs.join(' ')));
                m_setupPage->recheck();
                showSetupPage();
            }
            return;
        }
        if (pnpmArgs == QStringList{QStringLiteral("install")}) {
            m_logView->appendLog(QStringLiteral("> pnpm run build"));
            runBuildStep({QStringLiteral("run"), QStringLiteral("build")});
        } else {
            m_logView->appendLog(QStringLiteral("构建完成"));
            if (m_buildFromSetup) {
                // 构建来自引导页：回到引导页重新校验
                m_setupPage->recheck();
                showSetupPage();
            } else {
                startService();
            }
        }
    });
    // FailedToStart 时 finished 不会触发，单独兜底
    connect(proc, &QProcess::errorOccurred, this, [proc, pnpmArgs, this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            proc->deleteLater();
            m_logView->appendLog(QStringLiteral("pnpm 启动失败（pnpmPath 无效或权限不足）"), true);
            if (m_buildFromSetup) {
                QMessageBox::warning(this, QStringLiteral("构建失败"), QStringLiteral("pnpm 启动失败，请检查 pnpm 路径设置。"));
                m_setupPage->recheck();
                showSetupPage();
            }
        }
    });

    m_logView->appendLog(QStringLiteral("> pnpm %1").arg(pnpmArgs.join(' ')));
    startWrapped(proc, pnpm, pnpmArgs);
}

void MainWindow::onDshStateChanged(DshProcessManager::State state)
{
    qDebug() << "[UI] onDshStateChanged ->" << stateName(state);
    QString text;
    switch (state) {
    case DshProcessManager::State::Idle: text = QStringLiteral("未启动"); break;
    case DshProcessManager::State::Starting: text = QStringLiteral("启动中"); break;
    case DshProcessManager::State::Running:
        text = QStringLiteral("运行中");
        qDebug() << "[UI] Running -> 后台加载 webview（锚点就绪后再显示）port=" << m_settings.webPort;
        m_homePage->load(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_settings.webPort)));
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
