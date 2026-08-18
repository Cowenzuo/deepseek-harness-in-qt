#pragma once

#include <QString>

namespace dshinqt {

class AppSettings
{
public:
    QString sourcePath;
    int webPort = 3080;
    QString nodePath;
    QString pnpmPath;
    QString gitPath;
    QString repoUrl = QStringLiteral("https://github.com/deepseek-ai/deepseek-harness.git");

    bool load();
    bool save() const;

    static QString configFilePath();

    // 端口范围校验（可测纯函数）：非法值回退默认 3080
    static int clampPort(int port) { return (port >= 1 && port <= 65535) ? port : 3080; }

    // 服务 URL 统一拼接（探测/加载/展示共用，避免各处手拼字符串）
    QString webUrl() const { return QStringLiteral("http://127.0.0.1:%1").arg(webPort); }

    // git 可执行程序：显式路径优先，否则从 PATH 查找
    QString gitProgram() const { return gitPath.isEmpty() ? QStringLiteral("git") : gitPath; }
};

} // namespace dshinqt
