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
};

} // namespace dshinqt
