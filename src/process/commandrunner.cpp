#include "commandrunner.h"

#include <QDebug>
#include <QProcess>

#include "startwrapped.h"

namespace dshinqt {

CommandRunner::CommandRunner(QObject *parent)
    : QObject(parent)
{}

void CommandRunner::start(const QString &program, const QStringList &args, const QString &workingDirectory,
                          const QProcessEnvironment &env, std::function<void(bool success, int code)> onExit)
{
    if (m_proc) {
        qWarning() << "[CMD] 已有命令在运行，忽略新请求:" << program << args;
        emit outputReady(QStringLiteral("[内部] 已有命令在运行，忽略新请求\n"));
        return;
    }

    auto *proc = new QProcess(this);
    m_proc = proc;
    proc->setWorkingDirectory(workingDirectory);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    if (!env.isEmpty())
        proc->setProcessEnvironment(env);

    connect(proc, &QProcess::readyReadStandardOutput, this, [proc, this]() {
        emit outputReady(QString::fromUtf8(proc->readAllStandardOutput()));
    });
    // FailedToStart 时 finished 不会触发，需单独兜底
    connect(proc, &QProcess::errorOccurred, this, [proc, onExit, this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            m_proc = nullptr;
            proc->deleteLater();
            onExit(false, -1);
        }
    });
    connect(proc, &QProcess::finished, this, [proc, onExit, this](int code, QProcess::ExitStatus status) {
        if (m_proc == proc)
            m_proc = nullptr;
        proc->deleteLater();
        onExit(status == QProcess::NormalExit && code == 0, code);
    });

    startWrapped(proc, program, args); // .cmd/.bat/.ps1 shim 经 cmd.exe /c 包装
}

void CommandRunner::cancel()
{
    if (m_proc)
        m_proc->kill(); // finished 回调负责收尾与 onExit(false, ...)
}

} // namespace dshinqt
