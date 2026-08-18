#pragma once

#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <functional>

class QProcess;

namespace dshinqt {

// 统一封装 QProcess 异步命令执行：流式输出、FailedToStart 兜底、失败/成功回调。
// 同一时刻只运行一个任务；busy 时 start 被忽略并发出提示日志。
// 供 UpdateManager 流水线与 BuildFlowManager 复用，替代各处重复的 QProcess 接线。
class CommandRunner : public QObject
{
    Q_OBJECT

public:
    explicit CommandRunner(QObject *parent = nullptr);

    // 启动命令；成功/失败（含 FailedToStart 与被 kill）都会且只会触发一次 onExit
    void start(const QString &program, const QStringList &args, const QString &workingDirectory,
               const QProcessEnvironment &env, std::function<void(bool success, int code)> onExit);

    // 终止当前命令；其 finished 回调将触发 onExit(false, ...)
    void cancel();

    bool isBusy() const { return m_proc != nullptr; }

signals:
    // 命令的合并输出（stdout+stderr，逐块转发）
    void outputReady(const QString &text);

private:
    QProcess *m_proc = nullptr;
};

} // namespace dshinqt
