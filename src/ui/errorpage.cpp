#include "errorpage.h"
#include "ui/theme.h"

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace dshinqt {

ErrorPage::ErrorPage(QWidget *parent)
    : QWidget(parent)
{
    // 深色背景用 palette（可靠），QSS 只用于子控件
    setAutoFillBackground(true);
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, Theme::windowBg());
        setPalette(pal);
    }
    // 与引导页/设置弹窗一致的按钮体系（#primary/#secondary）
    setStyleSheet(QStringLiteral(R"(
#title { color: #f5f5f5; font-size: 26px; font-weight: 700; }
QLabel#msg { color: #e0e0e0; font-size: 13px; }
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
QScrollArea { background: transparent; border: none; }
)"));

    auto *title = new QLabel(QStringLiteral("dsh 未能启动"), this);
    title->setObjectName(QStringLiteral("title"));

    m_message = new QLabel(this);
    m_message->setObjectName(QStringLiteral("msg"));
    m_message->setWordWrap(true);

    // 消息区套滚动区：长错误明细与长路径可完整查看，不挤压按钮行
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(m_message);
    scroll->setMaximumHeight(180);

    m_retryBtn = new QPushButton(QStringLiteral("重试"), this);
    m_retryBtn->setObjectName(QStringLiteral("primary"));
    m_buildBtn = new QPushButton(QStringLiteral("一键构建依赖"), this);
    m_buildBtn->setObjectName(QStringLiteral("primary"));
    m_logBtn = new QPushButton(QStringLiteral("查看日志"), this);
    m_logBtn->setObjectName(QStringLiteral("secondary"));
    for (QPushButton *b : {m_retryBtn, m_buildBtn, m_logBtn}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::TabFocus);
    }

    connect(m_retryBtn, &QPushButton::clicked, this, &ErrorPage::retryRequested);
    connect(m_buildBtn, &QPushButton::clicked, this, &ErrorPage::buildRequested);
    connect(m_logBtn, &QPushButton::clicked, this, &ErrorPage::openLogRequested);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_retryBtn);
    buttons->addWidget(m_buildBtn);
    buttons->addWidget(m_logBtn);
    buttons->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(scroll);
    layout->addSpacing(8);
    layout->addLayout(buttons);
    layout->addStretch(1);

    setBuildVisible(false);
}

void ErrorPage::setMessage(const QString &msg)
{
    m_message->setText(msg);
}

void ErrorPage::setBuildVisible(bool visible)
{
    m_buildBtn->setVisible(visible);
}

} // namespace dshinqt
