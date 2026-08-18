#pragma once

#include <QWidget>

class QTextEdit;

class LogView : public QWidget
{
    Q_OBJECT

public:
    explicit LogView(QWidget *parent = nullptr);

    void appendLog(const QString &line, bool isError = false);
    void clearLog();

private:
    QTextEdit *m_text = nullptr;
};
