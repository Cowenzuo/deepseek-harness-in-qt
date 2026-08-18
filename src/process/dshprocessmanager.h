#pragma once

#include <QObject>
#include <QTimer>

#include <functional>

class AppSettings;
class ReadyWaiter;

// 端口反查结果：识别已在运行的服务（PID、node 路径、候选探测命中的源码根）
struct ServiceInfo
{
    bool ok = false;    // 端口上确认是 dsh 服务
    qint64 pid = 0;     // 监听进程 PID
    QString nodePath;   // node.exe 绝对路径
    QString sourceRoot; // 反查命中的 dsh 源码根（可能为空）
    QString commandLine;
};

// 常驻服务模型：dsh 由外壳分离启动（startDetached），外壳退出后继续运行。
// 外壳通过「异步端口探测 + 日志 tail」监督其状态，全程不阻塞主线程。
class DshProcessManager : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Starting, Running, Stopping, Crashed };
    Q_ENUM(State)

    explicit DshProcessManager(AppSettings *settings, QObject *parent = nullptr);
    ~DshProcessManager() override;

    void start();         // 强制启动（异步清理端口残留，再分离启动并监督）
    void attach();        // 服务已在运行：直接进入 Running
    void stop();          // 停止常驻服务
    void restart();       // 异步杀旧启新
    void ensureRunning(); // 异步探测端口：匹配则 attach，不一致则杀旧启新，不在则启动
    bool isRunning() const;
    State state() const { return m_state; }

    // 异步端口反查：PID + node 路径 + 尝试识别 dsh 源码根，回调在主线程执行
    void inspectAsync(std::function<void(const ServiceInfo &)> cb);

signals:
    void stateChanged(DshProcessManager::State state);
    void logOutput(const QString &line, bool isError);

private slots:
    void onWaiterReady();
    void onWaiterTimeout();
    void onPollTick();

private:
    void setState(State s);
    void beginLaunch();
    void killByPort(std::function<void()> onDone);
    void readLogTail();
    bool isServiceMatching(const QString &sourcePath) const;
    void writeServiceSource();
    void clearServiceSource();
    ServiceInfo parseServiceInfo(qint64 pid, const QString &powershellOut) const;

    AppSettings *m_settings = nullptr;
    ReadyWaiter *m_waiter = nullptr;
    QTimer m_pollTimer;
    State m_state = State::Idle;
    QString m_logPath;
    QString m_sourceFile;
    qint64 m_logPos = 0;
};
