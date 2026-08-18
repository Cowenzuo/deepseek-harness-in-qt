#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

namespace dshinqt {

class AppSettings;
class DshProcessManager;

// 设置弹窗「服务」页：状态灯、PID、端口/日志信息与启动/停止/重启控制（含危险操作确认）。
class ServiceSettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit ServiceSettingsPage(AppSettings *settings, DshProcessManager *proc, QWidget *parent = nullptr);

private:
    void refreshServiceUi();
    void onStartService();
    void onStopService();
    void onRestartService();
    bool confirmServiceInterrupt(const QString &action);

    AppSettings *m_settings = nullptr;
    DshProcessManager *m_proc = nullptr;
    QLabel *m_svcStatusLabel = nullptr;
    QLabel *m_svcPidLabel = nullptr;
    QLabel *m_svcSourceLabel = nullptr;
    QPushButton *m_svcStartBtn = nullptr;
    QPushButton *m_svcStopBtn = nullptr;
    QPushButton *m_svcRestartBtn = nullptr;
};

} // namespace dshinqt
