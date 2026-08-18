#include "logview.h"
#include "ui/theme.h"

#include <QColor>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QPalette>
#include <QPushButton>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

namespace dshinqt {

LogView::LogView(QWidget *parent)
    : QWidget(parent)
{
    // 深色背景用 palette（可靠），QSS 只用于子控件
    setAutoFillBackground(true);
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, Theme::windowBg());
        setPalette(pal);
    }
    // 顶部工具按钮：与全局 #secondary 风格一致的深色蓝系
    setStyleSheet(QStringLiteral(R"(
QPushButton#logTool {
    background: #2c2c31; border: 1px solid #38383e; border-radius: 6px;
    padding: 4px 12px; color: #9a9aa0; font-size: 12px;
}
QPushButton#logTool:hover { background: #35353b; border-color: #4f8cff; color: #ffffff; }
QPushButton#logTool:pressed { background: #2f3550; }
)"));

    // 顶部工具行：返回主页 + 清空
    auto *backBtn = new QPushButton(QStringLiteral("← 返回主页"), this);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setFocusPolicy(Qt::TabFocus);
    backBtn->setObjectName(QStringLiteral("logTool"));
    auto *clearBtn = new QPushButton(QStringLiteral("清空"), this);
    clearBtn->setCursor(Qt::PointingHandCursor);
    clearBtn->setFocusPolicy(Qt::TabFocus);
    clearBtn->setObjectName(QStringLiteral("logTool"));
    connect(backBtn, &QPushButton::clicked, this, &LogView::backRequested);
    connect(clearBtn, &QPushButton::clicked, this, &LogView::clearLog);

    auto *toolRow = new QHBoxLayout;
    toolRow->setContentsMargins(8, 6, 8, 6);
    toolRow->setSpacing(8);
    toolRow->addWidget(backBtn);
    toolRow->addWidget(clearBtn);
    toolRow->addStretch(1);

    m_text = new QTextEdit(this);
    m_text->setReadOnly(true);
    m_text->setLineWrapMode(QTextEdit::WidgetWidth);

    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPointSize(9);
    m_text->setFont(f);

    // 深色终端配色（用 palette，QSS 对 QTextEdit 文本颜色不可靠）
    QPalette pal = m_text->palette();
    pal.setColor(QPalette::Base, Theme::windowBg());
    pal.setColor(QPalette::Text, QColor(200, 200, 200));
    m_text->setPalette(pal);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(toolRow);
    layout->addWidget(m_text);
}

void LogView::appendLog(const QString &line, bool isError)
{
    // 行数上限：超出时裁剪文档头部，避免长会话内存无界增长
    static const int kMaxBlocks = 5000;
    if (m_text->document()->blockCount() > kMaxBlocks) {
        QTextCursor cur(m_text->document());
        cur.movePosition(QTextCursor::Start);
        cur.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 1000);
        cur.removeSelectedText();
    }

    // 规范化换行：先合并 \r\n，再把进度覆盖符 \r 转成换行，避免多余空行
    QString text = line;
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    // insertText 不会自动换行，非空且不以换行结尾时补一个换行
    if (!text.isEmpty() && !text.endsWith(QLatin1Char('\n')))
        text += QLatin1Char('\n');

    QTextCharFormat fmt;
    fmt.setForeground(isError ? QColor(255, 107, 107) : QColor(200, 200, 200));

    QTextCursor cursor(m_text->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text, fmt);

    m_text->setTextCursor(cursor);
    m_text->ensureCursorVisible();
}

void LogView::clearLog()
{
    m_text->clear();
}

} // namespace dshinqt
