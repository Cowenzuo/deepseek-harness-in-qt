#include "servicepage.h"

#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>

#include "appsettings.h"
#include "process/dshprocessmanager.h"

namespace dshinqt {

ServiceSettingsPage::ServiceSettingsPage(AppSettings *settings, DshProcessManager *proc, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_proc(proc)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(24, 20, 24, 20);
    v->setSpacing(16);

    // 状态 + 要素卡片
    auto *card = new QFrame(this);
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
    m_svcStartBtn = new QPushButton(QStringLiteral("启动"), this);
    m_svcStartBtn->setObjectName(QStringLiteral("primary"));
    m_svcStopBtn = new QPushButton(QStringLiteral("停止"), this);
    m_svcStopBtn->setObjectName(QStringLiteral("secondary"));
    m_svcRestartBtn = new QPushButton(QStringLiteral("重启"), this);
    m_svcRestartBtn->setObjectName(QStringLiteral("secondary"));
    for (QPushButton *b : {m_svcStartBtn, m_svcStopBtn, m_svcRestartBtn}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
    }
    connect(m_svcStartBtn, &QPushButton::clicked, this, &ServiceSettingsPage::onStartService);
    connect(m_svcStopBtn, &QPushButton::clicked, this, &ServiceSettingsPage::onStopService);
    connect(m_svcRestartBtn, &QPushButton::clicked, this, &ServiceSettingsPage::onRestartService);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(12);
    btnRow->addWidget(m_svcStartBtn);
    btnRow->addWidget(m_svcStopBtn);
    btnRow->addWidget(m_svcRestartBtn);
    btnRow->addStretch(1);

    v->addWidget(card);
    v->addLayout(btnRow);
    v->addStretch(1);

    connect(m_proc, &DshProcessManager::stateChanged, this, [this](DshProcessManager::State) { refreshServiceUi(); });
    refreshServiceUi();

    // 打开时反查一次：填充 PID / 源码路径（QPointer 防页面先于回调析构）
    QPointer<ServiceSettingsPage> guard(this);
    m_proc->inspectAsync([guard](const ServiceInfo &info) {
        if (!guard)
            return;
        if (info.ok) {
            guard->m_svcPidLabel->setText(info.pid > 0 ? QStringLiteral("PID：%1").arg(info.pid)
                                                       : QStringLiteral("PID：未知"));
            if (!info.sourceRoot.isEmpty())
                guard->m_svcSourceLabel->setText(QStringLiteral("源码：%1").arg(info.sourceRoot));
        } else {
            guard->m_svcPidLabel->setText(QStringLiteral("PID：无（端口无服务）"));
        }
    });
}

void ServiceSettingsPage::refreshServiceUi()
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

void ServiceSettingsPage::onStartService()
{
    m_proc->start(); // 异步清理端口残留后启动
}

void ServiceSettingsPage::onStopService()
{
    if (!confirmServiceInterrupt(QStringLiteral("停止服务")))
        return;
    m_proc->stop();
}

void ServiceSettingsPage::onRestartService()
{
    if (!confirmServiceInterrupt(QStringLiteral("重启服务")))
        return;
    m_proc->restart();
}

bool ServiceSettingsPage::confirmServiceInterrupt(const QString &action)
{
    return QMessageBox::question(this, QStringLiteral("确认"),
                                 QStringLiteral("%1 将终止当前 dsh 服务，正在进行的对话会立即中断。继续？")
                                     .arg(action),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
           == QMessageBox::Yes;
}

} // namespace dshinqt
