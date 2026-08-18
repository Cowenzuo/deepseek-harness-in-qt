#include "startwrapped.h"

namespace dshinqt {

#ifdef Q_OS_WIN

#include <windows.h>

#include <vector>

// Windows：用 CreateProcessW + DETACHED_PROCESS 分离启动（真正的后台进程）。
// DETACHED_PROCESS：不继承、不新建控制台，关闭外壳/终端都不影响；
// CREATE_NEW_PROCESS_GROUP：隔离 Ctrl+C。
// 可执行文件（.exe）直接启动，不经过 cmd.exe；仅 .cmd/.bat/.ps1 shim 才用 cmd.exe 包装。
// stdout/stderr 通过 STARTUPINFO 句柄直接重定向到 logFile。
bool startDetachedWrapped(const QString &program, const QStringList &args, const QString &workingDirectory,
                          const QString &logFile)
{
    QString cmd = shellQuote(program);
    for (const QString &a : args)
        cmd += QLatin1Char(' ') + shellQuote(a);

    const QString ext = QFileInfo(program).suffix().toLower();
    const bool isShim =
        ext.isEmpty() || ext == QStringLiteral("cmd") || ext == QStringLiteral("bat") || ext == QStringLiteral("ps1");
    const QString fullCmd = isShim ? QStringLiteral("cmd.exe /c ") + cmd : cmd;

    // 可继承句柄的安全属性
    SECURITY_ATTRIBUTES sa;
    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    // 日志文件：stdout/stderr 的重定向目标
    const std::wstring logPath = QDir::toNativeSeparators(logFile).toStdWString();
    HANDLE hLog = CreateFileW(logPath.c_str(),
                              GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              &sa,
                              CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);

    HANDLE hNul =
        CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hLog == INVALID_HANDLE_VALUE)
        hLog = hNul; // 日志打不开时丢弃输出，不阻断启动

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = hNul;
    si.hStdOutput = hLog;
    si.hStdError = hLog;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::wstring cmdLine = fullCmd.toStdWString();
    std::vector<wchar_t> buf(cmdLine.begin(), cmdLine.end());
    buf.push_back(L'\0');

    std::wstring wd = QDir::toNativeSeparators(workingDirectory).toStdWString();

    const BOOL ok = CreateProcessW(nullptr,    // lpApplicationName（从命令行推断）
                                   buf.data(), // lpCommandLine（可写缓冲）
                                   nullptr,
                                   nullptr, // 进程/线程安全属性
                                   TRUE,    // 继承句柄（stdout/stderr 重定向可传播）
                                   DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
                                   nullptr, // 环境（继承）
                                   wd.empty() ? nullptr : wd.c_str(),
                                   &si,
                                   &pi);

    if (hLog != INVALID_HANDLE_VALUE && hLog != hNul)
        CloseHandle(hLog); // hLog 回退到 hNul 时避免重复关闭
    if (hNul != INVALID_HANDLE_VALUE)
        CloseHandle(hNul);

    if (ok) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return ok != FALSE;
}

#else

// 非 Windows：/bin/sh -c 分离启动
bool startDetachedWrapped(const QString &program, const QStringList &args, const QString &workingDirectory,
                          const QString &logFile)
{
    QString cmd = shellQuote(program);
    for (const QString &a : args)
        cmd += QLatin1Char(' ') + shellQuote(a);
    cmd += QStringLiteral(" > ") + shellQuote(logFile) + QStringLiteral(" 2>&1");
    return QProcess::startDetached(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), cmd}, workingDirectory);
}

#endif

} // namespace dshinqt
