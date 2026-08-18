#include "startuppage.h"
#include "ui/theme.h"

#include <QColor>
#include <QLabel>
#include <QPalette>
#include <QProgressBar>
#include <QVBoxLayout>

namespace dshinqt {

StartupPage::StartupPage(QWidget *parent)
    : QWidget(parent)
{
    // 深色背景用 palette（可靠），QSS 只用于子控件
    setAutoFillBackground(true);
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, Theme::windowBg());
        setPalette(pal);
    }

    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(24);

    auto *title = new QLabel(QStringLiteral("正在启动 deepseek-harness ..."), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("color:#f5f5f5; font-size:20px; font-weight:600;"));

    auto *bar = new QProgressBar(this);
    bar->setRange(0, 0); // 不确定进度
    bar->setTextVisible(false);
    bar->setFixedWidth(320);
    bar->setStyleSheet(QStringLiteral("QProgressBar { background:#26262a; border:none; border-radius:4px; height:6px; }"
                                      "QProgressBar::chunk { background:#4f8cff; border-radius:4px; }"));

    m_statusLabel = new QLabel(QStringLiteral("正在检测环境..."), this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#9a9a9a; font-size:14px;"));

    layout->addWidget(title);
    layout->addWidget(bar, 0, Qt::AlignHCenter);
    layout->addWidget(m_statusLabel);
}

void StartupPage::setStatus(const QString &text)
{
    m_statusLabel->setText(text);
}

} // namespace dshinqt
