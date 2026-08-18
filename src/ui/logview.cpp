#include "logview.h"

#include <QColor>
#include <QFontDatabase>
#include <QPalette>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

LogView::LogView(QWidget *parent)
    : QWidget(parent)
{
    // 深色背景用 palette（可靠）
    setAutoFillBackground(true);
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(18, 18, 18));
        setPalette(pal);
    }

    m_text = new QTextEdit(this);
    m_text->setReadOnly(true);
    m_text->setLineWrapMode(QTextEdit::WidgetWidth);

    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPointSize(9);
    m_text->setFont(f);

    // 深色终端配色（用 palette，QSS 对 QTextEdit 文本颜色不可靠）
    QPalette pal = m_text->palette();
    pal.setColor(QPalette::Base, QColor(18, 18, 18));
    pal.setColor(QPalette::Text, QColor(200, 200, 200));
    m_text->setPalette(pal);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_text);
}

void LogView::appendLog(const QString &line, bool isError)
{
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
