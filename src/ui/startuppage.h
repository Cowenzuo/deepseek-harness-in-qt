#pragma once

#include <QWidget>

class QLabel;

namespace dshinqt {

// 启动加载页：启动首屏，显示「检测环境 / 启动服务」状态与进度条。
class StartupPage : public QWidget
{
    Q_OBJECT

public:
    explicit StartupPage(QWidget *parent = nullptr);

    void setStatus(const QString &text);

private:
    QLabel *m_statusLabel = nullptr;
};

} // namespace dshinqt
