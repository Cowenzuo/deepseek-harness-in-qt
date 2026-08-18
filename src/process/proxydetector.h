#pragma once

#include <QString>
#include <QStringList>

namespace dshinqt {

// 检测系统代理并转成 git 命令行参数。
// git 不读 Windows 系统代理，需显式注入 http.proxy / https.proxy。
class ProxyDetector
{
public:
    // 系统代理地址（如 http://127.0.0.1:7897）；未启用返回空
    static QString systemProxy();

    // git 代理参数（如 -c http.proxy=... -c https.proxy=...）；无代理返回空
    static QStringList gitProxyArgs();

    // 把 host:port 补全成带协议的代理 URL
    static QString toProxyUrl(const QString &hostPort);
};

} // namespace dshinqt
