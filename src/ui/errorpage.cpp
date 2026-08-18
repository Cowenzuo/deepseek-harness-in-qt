#include "errorpage.h"

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>

ErrorPage::ErrorPage(QWidget *parent)
    : QWidget(parent)
{
    // 深色背景用 palette（可靠），QSS 只用于子控件
    setAutoFillBackground(true);
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(18, 18, 18));
        setPalette(pal);
    }
    setStyleSheet(QStringLiteral("ErrorPage { background: #121212; }"
                                 "QLabel { color: #e0e0e0; }"));

    auto *title = new QLabel(QStringLiteral("dsh 未能启动"), this);
    QFont f = title->font();
    f.setPointSize(f.pointSize() + 4);
    f.setBold(true);
    title->setFont(f);

    m_message = new QLabel(this);
    m_message->setWordWrap(true);

    m_retryBtn = new QPushButton(QStringLiteral("重试"), this);
    m_buildBtn = new QPushButton(QStringLiteral("一键构建"), this);
    m_logBtn = new QPushButton(QStringLiteral("查看日志"), this);

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
    layout->addWidget(m_message);
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
