#pragma once

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace dshinqt {

// 命令行参数引号化：含空格/双引号时用双引号包裹并转义内嵌双引号（CRT 命令行解析规则）
inline QString shellQuote(const QString &s)
{
    if (!s.contains(QLatin1Char(' ')) && !s.contains(QLatin1Char('"')))
        return s;
    QString escaped = s;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

// Windows 下 pnpm/npm 等以 .cmd/.bat/.ps1 shim 形式存在，QProcess 无法直接执行，
// 统一通过 cmd.exe /c 包装执行；有明确扩展名的可执行文件（node.exe、git.exe）直接启动。
// 其他平台直接启动。
inline void startWrapped(QProcess *proc, const QString &program, const QStringList &args)
{
#ifdef Q_OS_WIN
    const QString ext = QFileInfo(program).suffix().toLower();
    const bool isShim =
        ext.isEmpty() || ext == QStringLiteral("cmd") || ext == QStringLiteral("bat") || ext == QStringLiteral("ps1");
    if (isShim) {
        QString cmd = shellQuote(program); // 程序路径也需引号化（含空格路径如 C:\Program Files\...）
        for (const QString &a : args)
            cmd += QLatin1Char(' ') + shellQuote(a);
        proc->start(QStringLiteral("cmd.exe"), {QStringLiteral("/c"), cmd});
        return;
    }
#endif
    proc->start(program, args);
}

// 分离启动（常驻服务）：DETACHED_PROCESS 使进程完全脱离控制台，外壳退出后继续运行。
// stdout/stderr 通过句柄重定向到 logFile，供外壳 tail 检测 boot 失败。
// Windows 实现见 startdetached.cpp：可执行文件（.exe）直接启动，shim 用 cmd.exe 包装。
bool startDetachedWrapped(const QString &program, const QStringList &args, const QString &workingDirectory,
                          const QString &logFile);

} // namespace dshinqt
