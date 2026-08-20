#pragma once

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace dshinqt {

class AppSettings;
class CommandRunner;

// 设置弹窗「会话修复」页：输入损坏会话 GUID，调用内置 node 修复脚本
// （bin/tools/session-repair.mts）诊断/修复 seq 重复或断裂的 JSONL 会话日志。
// 脚本复用 dsh 官方模块（zstd 帧 + decodeStorageRecord），修复前自动备份，
// 修复后经严格走查与加载器同源校验，失败还原；活动会话拒绝修复。
class SessionRepairPage : public QWidget
{
    Q_OBJECT

public:
    explicit SessionRepairPage(AppSettings *settings, QWidget *parent = nullptr);

private slots:
    void runDiagnose();
    void runRepair();
    void onRunnerOutput(const QString &text);
    void onRunnerDone(bool success, int code);

private:
    void runAction(const QString &action, const QString &confirmTitle, const QString &confirmText);
    void appendOutput(const QString &line, bool isError = false);
    void setBusy(bool busy);
    QString scriptPath() const;

    AppSettings *m_settings = nullptr;
    CommandRunner *m_runner = nullptr;
    QLineEdit *m_guidEdit = nullptr;
    QPushButton *m_diagnoseBtn = nullptr;
    QPushButton *m_repairBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QPlainTextEdit *m_output = nullptr;
};

} // namespace dshinqt
