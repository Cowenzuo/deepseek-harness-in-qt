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

    void setMessage(const QString &msg); // 错误详情
    void setBuildVisible(bool visible);  // 是否显示「一键构建」按钮

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
