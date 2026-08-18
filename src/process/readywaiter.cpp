#include "readywaiter.h"

#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace dshinqt {

ReadyWaiter::ReadyWaiter(QObject *parent)
    : QObject(parent)
{
    m_probeTimer.setInterval(1000);
    connect(&m_probeTimer, &QTimer::timeout, this, &ReadyWaiter::probe);

    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &ReadyWaiter::timeout);

    connect(&m_nam, &QNetworkAccessManager::finished, this, &ReadyWaiter::onFinished);
}

void ReadyWaiter::wait(const QString &host, int port, int timeoutMs)
{
    qDebug() << "[WAIT] wait() 开始轮询" << host << port << "timeoutMs=" << timeoutMs;
    m_url = QUrl(QStringLiteral("http://%1:%2").arg(host).arg(port));
    m_waitActive = true;
    m_timeoutTimer.start(timeoutMs);
    probe();
    m_probeTimer.start();
}

void ReadyWaiter::probeOnce(const QString &host, int port, std::function<void(bool)> onResult)
{
    qDebug() << "[WAIT] probeOnce()" << host << port;
    const QUrl url = QUrl(QStringLiteral("http://%1:%2").arg(host).arg(port));
    QNetworkRequest req(url);
    req.setTransferTimeout(300);
    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, onResult]() {
        const bool ok = reply->error() == QNetworkReply::NoError;
        qDebug() << "[WAIT] probeOnce 结果 ok=" << ok << "error=" << reply->error();
        reply->deleteLater();
        onResult(ok);
    });
}

void ReadyWaiter::stop()
{
    m_probeTimer.stop();
    m_timeoutTimer.stop();
    m_waitActive = false;
}

void ReadyWaiter::probe()
{
    QNetworkRequest req(m_url);
    req.setTransferTimeout(2000);
    m_nam.get(req);
}

void ReadyWaiter::onFinished(QNetworkReply *reply)
{
    const QByteArray body = reply->readAll();
    const bool netOk = reply->error() == QNetworkReply::NoError;
    reply->deleteLater();

    if (!m_waitActive)
        return; // probeOnce 的响应由它自己的回调处理

    // 打印每次 200 响应的页面特征，用于确认正常/未就绪页面的真实差异
    if (netOk) {
        const QString html = QString::fromUtf8(body);
        QString title;
        const int t0 = html.indexOf(QStringLiteral("<title>"));
        if (t0 >= 0)
            title = html.mid(t0 + 7).left(60).section(QLatin1Char('<'), 0, 0);
        qDebug() << "[WAIT] 200 len=" << body.size() << "boot=" << body.contains("__DSH_BOOT__")
                 << "failedLoad=" << body.toLower().contains("failed to load plugins") << "title=" << title;
    }

    const bool isReady = netOk && isDshReady(body);
    qDebug() << "[WAIT] probe 结果 ready=" << isReady;
    if (isReady) {
        stop();
        emit ready();
    }
    // 未就绪：继续轮询（每秒重试，事件驱动，无固定延时），直到超时
}

bool ReadyWaiter::isDshReady(const QByteArray &body) const
{
    return dshBootInBody(body);
}

} // namespace dshinqt
