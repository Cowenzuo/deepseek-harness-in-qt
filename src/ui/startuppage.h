#pragma once

#include <QWidget>

class QLabel;

// 启动加载页：启动首屏，显示「检测环境 / 启动服务」状态与进度条。
class StartupPage : public QWidget
{
    Q_OBJECT

public:
    explicit StartupPage(QWidget *parent = nullptr);

    void setStatus(const QString &text); // 更新状态文字

private:
    QLabel *m_statusLabel = nullptr;
};
