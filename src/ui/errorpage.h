#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

// 错误页：环境/依赖/服务启动受阻时展示原因与操作入口。
class ErrorPage : public QWidget
{
    Q_OBJECT

public:
    explicit ErrorPage(QWidget *parent = nullptr);

    void setMessage(const QString &msg);
    void setBuildVisible(bool visible);

signals:
    void retryRequested();
    void buildRequested();
    void openLogRequested();

private:
    QLabel *m_message = nullptr;
    QPushButton *m_retryBtn = nullptr;
    QPushButton *m_buildBtn = nullptr;
    QPushButton *m_logBtn = nullptr;
};
