#include "proxydetector.h"

#include <QSettings>

QString ProxyDetector::toProxyUrl(const QString &hostPort)
{
    QString s = hostPort.trimmed();
    if (s.isEmpty())
        return {};
    if (s.contains(QStringLiteral("://")))
        return s;
    return QStringLiteral("http://") + s;
}

QString ProxyDetector::systemProxy()
{
#ifdef Q_OS_WIN
    // Windows 系统代理在注册表（Clash/V2Ray 等会把 ProxyEnable 置 1）
    QSettings settings(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"),
        QSettings::NativeFormat);
    if (settings.value(QStringLiteral("ProxyEnable"), 0).toInt() == 0)
        return {};
    const QString server = settings.value(QStringLiteral("ProxyServer")).toString().trimmed();
    if (server.isEmpty())
        return {};

    // 分号分隔的多协议配置（如 http=...;https=...;socks=...），优先 http/https
    const QStringList parts = server.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const int eq = part.indexOf(QLatin1Char('='));
        if (eq > 0) {
            const QString scheme = part.left(eq).trimmed().toLower();
            if (scheme == QStringLiteral("http") || scheme == QStringLiteral("https"))
                return toProxyUrl(part.mid(eq + 1).trimmed());
        }
    }
    // 无协议标记，整体作为 http 代理
    return toProxyUrl(server);
#else
    // Unix/macOS：代理工具一般通过环境变量暴露
    static const char *keys[] = {"HTTPS_PROXY", "https_proxy", "HTTP_PROXY", "http_proxy", "ALL_PROXY", "all_proxy"};
    for (const char *k : keys) {
        const QString v = qEnvironmentVariable(k);
        if (!v.isEmpty())
            return toProxyUrl(v);
    }
    return {};
#endif
}

QStringList ProxyDetector::gitProxyArgs()
{
    const QString proxy = systemProxy();
    if (proxy.isEmpty())
        return {};
    return {QStringLiteral("-c"),
            QStringLiteral("http.proxy=") + proxy,
            QStringLiteral("-c"),
            QStringLiteral("https.proxy=") + proxy};
}
