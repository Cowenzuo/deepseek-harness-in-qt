#include "appsettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace dshinqt {

namespace {
const char *kSourcePath = "sourcePath";
const char *kWebPort = "webPort";
const char *kNodePath = "nodePath";
const char *kPnpmPath = "pnpmPath";
const char *kGitPath = "gitPath";
const char *kRepoUrl = "repoUrl";
} // namespace

QString AppSettings::configFilePath()
{
    // 固定放在可执行目录下的 config/config.json（绿色软件，跟随 exe）
    const QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/config");
    return QDir(dir).filePath(QStringLiteral("config.json"));
}

bool AppSettings::load()
{
    QFile f(configFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return false; // 无配置，保持默认值

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isNull() || !doc.isObject())
        return false; // 配置文件损坏，保持默认值
    const QJsonObject o = doc.object();
    sourcePath = o.value(QLatin1String(kSourcePath)).toString(sourcePath);
    {
        // 端口范围校验：手改/损坏的配置可能带 0、负数或超范围值，回退默认
        const int port = o.value(QLatin1String(kWebPort)).toInt(webPort);
        webPort = clampPort(port);
    }
    nodePath = o.value(QLatin1String(kNodePath)).toString(nodePath);
    pnpmPath = o.value(QLatin1String(kPnpmPath)).toString(pnpmPath);
    gitPath = o.value(QLatin1String(kGitPath)).toString(gitPath);
    repoUrl = o.value(QLatin1String(kRepoUrl)).toString(repoUrl);
    return true;
}

bool AppSettings::save() const
{
    QJsonObject o;
    o.insert(QLatin1String(kSourcePath), sourcePath);
    o.insert(QLatin1String(kWebPort), webPort);
    o.insert(QLatin1String(kNodePath), nodePath);
    o.insert(QLatin1String(kPnpmPath), pnpmPath);
    o.insert(QLatin1String(kGitPath), gitPath);
    o.insert(QLatin1String(kRepoUrl), repoUrl);

    const QString path = configFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const QByteArray data = QJsonDocument(o).toJson(QJsonDocument::Indented);
    return f.write(data) == data.size();
}

} // namespace dshinqt
