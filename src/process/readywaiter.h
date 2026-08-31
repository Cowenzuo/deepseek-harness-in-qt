#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QUrl>

#include <functional>

class QNetworkReply;

namespace dshinqt {

// 服务层就绪判定（可测纯函数）：页面含 dsh boot 数据（__DSH_BOOT__）即就绪（正面信号）。
// 界面层（插件激活/聊天界面挂载）由 HomePage 的正面锚点轮询判定，此处不枚举 warning。
inline bool dshBootInBody(const QByteArray &body)
{
    return body.contains("__DSH_BOOT__");
}

class ReadyWaiter : public QObject
{
    Q_OBJECT

public:
    explicit ReadyWaiter(QObject *parent = nullptr);

    void wait(const QString &host, int port, int timeoutMs = 60000);
    void stop();

    // 单次探测端口：一次 HTTP GET，结果通过回调返回（非阻塞）
    void probeOnce(const QString &host, int port, std::function<void(bool)> onResult);

    // dsh web 认证 token（服务启动后从日志提取）：就绪探测携带 token，
    // 否则新版 dsh（token 认证）对无 token 请求一律 401，永不就绪
    void setToken(const QString &token) { m_token = token; }

signals:
    void ready();
    void timeout();

private slots:
    void probe();
    void onFinished(QNetworkReply *reply);

private:
    // 真正的就绪判定：页面含 dsh boot 数据（__DSH_BOOT__）即服务层就绪（正面信号）。
    // 界面层（插件激活/聊天界面挂载）由 HomePage 的正面锚点轮询判定，此处不枚举 warning。
    bool isDshReady(const QByteArray &body) const;

    QNetworkAccessManager m_nam;
    QTimer m_probeTimer;
    QTimer m_timeoutTimer;
    QUrl m_url;
    QString m_token;      // dsh web 认证 token（setToken 注入）
    bool m_waitActive = false; // 区分 wait() 轮询与 probeOnce 单次探测
};

} // namespace dshinqt
