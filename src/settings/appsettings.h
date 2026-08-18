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

    // git 可执行程序：显式路径优先，否则从 PATH 查找
    QString gitProgram() const { return gitPath.isEmpty() ? QStringLiteral("git") : gitPath; }
};

} // namespace dshinqt
