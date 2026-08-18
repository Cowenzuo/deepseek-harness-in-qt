#pragma once

#include <QWidget>

class QTextEdit;

namespace dshinqt {

class LogView : public QWidget
{
    Q_OBJECT

public:
    explicit LogView(QWidget *parent = nullptr);

    void appendLog(const QString &line, bool isError = false);
    void clearLog();

signals:
    void backRequested(); // 顶部「返回主页」按钮点击

private:
    QTextEdit *m_text = nullptr;
};

} // namespace dshinqt
