#include "homepage.h"
#include "settings/appsettings.h"
#include "ui/theme.h"

#include <QBoxLayout>
#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPalette>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>
#include <QWebEngineDownloadRequest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>

namespace dshinqt {

namespace {


// 正面就绪锚点：输入区(data-composer-card) + 会话区(data-conversation-scroll) 均挂载
// = 聊天界面真正可用。只判“该出现的东西出现没有”，不枚举任何 warning。
const char kJsReadyCheck[] = "!!document.querySelector('[data-composer-card]')"
                             " && !!document.querySelector('[data-conversation-scroll]')";

const int kCheckIntervalMs = 500;  // 轮询间隔（事件驱动反压，非固定延时）
const int kReloadAfterMs = 10000;  // 单周期未命中 → 自愈 reload（对应 reload 能自愈的现象）
const int kTotalTimeoutMs = 60000; // 总超时 → pageFailed
} // namespace

HomePage::HomePage(AppSettings *settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Theme::windowBg());
    setPalette(pal);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    // 在窗口 show 之前同步创建 webview：让顶层窗口第一帧就以 RHI/OpenGL 形态创建，
    // 避免显示后切换合成后端导致整窗重建白闪。webview 常驻渲染（被 stackwidget 盖住）。
    ensureWebView();
}

void HomePage::ensureWebView()
{
    if (m_webView)
        return;

    m_webView = new QWebEngineView(this);
    // 不透明深色背景（匹配界面底色）：不透明才不触发 WA_AlwaysStackOnTop（打洞穿透），
    // 才能被上层普通 widget 盖住；同时 Chromium 首帧前显示深色而非白。
    m_webView->page()->setBackgroundColor(Theme::windowBg());
    m_webView->setAutoFillBackground(true);
    {
        QPalette webPal = m_webView->palette();
        webPal.setColor(QPalette::Window, Theme::windowBg());
        webPal.setColor(QPalette::Base, Theme::windowBg());
        m_webView->setPalette(webPal);
    }
    m_webView->setStyleSheet(QStringLiteral("QWebEngineView { background: #121212; }"));

    // 下载承接：QtWebEngine 无默认下载行为，不接 downloadRequested 会被静默取消。
    // 接一次 profile 信号全局生效：按设置下载目录落盘，进度/结果转发日志页。
    // 目录每次下载时实时读 settings——设置里改了路径立即生效，无需重启。
    {
        auto *profile = m_webView->page()->profile();
        connect(profile, &QWebEngineProfile::downloadRequested, this,
                [this](QWebEngineDownloadRequest *download) {
            QString name = download->suggestedFileName();
            if (name.isEmpty())
                name = QFileInfo(download->url().path()).fileName(); // 兜底：URL 末段文件名
            QString dir = m_settings->downloadDirectory();
            if (!QDir().mkpath(dir)) {
                // 配置目录不可创建 → 退回系统下载目录，避免 accept 后落盘失败
                emit downloadLog(QStringLiteral("[下载] 目录不可用，改用系统下载目录：%1").arg(dir), true);
                dir = AppSettings::defaultDownloadDirectory();
                QDir().mkpath(dir);
            }
            download->setDownloadDirectory(dir);
            download->setDownloadFileName(name);
            emit downloadLog(QStringLiteral("[下载] %1 -> %2").arg(name, dir), false);
            download->accept(); // 不调用 accept() = 取消下载

            // 结果跟踪：完成/取消/中断（DownloadRequested/InProgress 忽略）
            connect(download, &QWebEngineDownloadRequest::stateChanged, this,
                    [this, name, dir](QWebEngineDownloadRequest::DownloadState state) {
                switch (state) {
                case QWebEngineDownloadRequest::DownloadCompleted:
                    emit downloadLog(QStringLiteral("[下载] 完成：%1").arg(QDir(dir).filePath(name)), false);
                    break;
                case QWebEngineDownloadRequest::DownloadCancelled:
                case QWebEngineDownloadRequest::DownloadInterrupted:
                    emit downloadLog(QStringLiteral("[下载] 已取消/失败：%1").arg(name), true);
                    break;
                default:
                    break;
                }
            });
        });
        qDebug() << "[UI] 下载信号已接入 profile=" << profile;
    }

    qobject_cast<QVBoxLayout *>(layout())->addWidget(m_webView); // ensureWebView 仅在构造末尾调用，layout 必为 QVBoxLayout

    // 每次加载完成：先抓取渲染后的完整 body 保存到 config/webview-latest.html 供对比（调试，覆盖写），
    // 再启动正面锚点轮询，命中（输入区+会话区挂载）才认为界面就绪。
    connect(m_webView, &QWebEngineView::loadFinished, this, [this](bool ok) {
        qDebug() << "[UI] webview loadFinished ok=" << ok;
        m_webView->page()->toHtml([this](const QString &html) {
            const QString path =
                QCoreApplication::applicationDirPath() + QStringLiteral("/config/webview-latest.html");
            QFile f(path);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(html.toUtf8());
                f.close();
            }
            qDebug() << "[UI] webview body len=" << html.size() << "saved=" << path;
        });
        // 自愈 reload 也触发 loadFinished，此处不重置总超时计时与自愈标记：
        // 总超时以 load() 为基准，自愈只允许一次，避免无限 reload
        startReadyCheck();
    });
    qDebug() << "[UI] HomePage webview 创建完成";
}

void HomePage::startReadyCheck()
{
    if (!m_readyTimer) {
        m_readyTimer = new QTimer(this);
        m_readyTimer->setInterval(kCheckIntervalMs);
        connect(m_readyTimer, &QTimer::timeout, this, &HomePage::checkReadyOnce);
    }
    checkReadyOnce(); // 立即查一次（页面可能已就绪）
    m_readyTimer->start();
}

void HomePage::checkReadyOnce()
{
    if (!m_webView)
        return;
    m_webView->page()->runJavaScript(QString::fromLatin1(kJsReadyCheck), [this](const QVariant &v) {
        const bool hit = v.toBool();
        if (hit) {
            qDebug() << "[UI] 正面锚点命中（输入区+会话区已挂载），页面就绪";
            if (m_readyTimer)
                m_readyTimer->stop();
            emit pageReady();
            return;
        }
        const qint64 elapsed = m_loadStart.elapsed();
        const qint64 cycle = elapsed - m_cycleStartMs;
        if (!m_reloaded && cycle >= kReloadAfterMs) {
            qDebug() << "[UI] 单周期" << kReloadAfterMs << "ms 未命中锚点，自愈 reload";
            m_reloaded = true;
            m_cycleStartMs = m_loadStart.elapsed();
            m_webView->reload();
            return;
        }
        if (elapsed >= kTotalTimeoutMs) {
            qDebug() << "[UI] 总超时" << kTotalTimeoutMs << "ms 锚点未出现，判页面失败";
            if (m_readyTimer)
                m_readyTimer->stop();
            emit pageFailed();
            return;
        }
    });
}

void HomePage::load(const QUrl &url)
{
    ensureWebView();
    // 总超时与自愈标记以每次显式 load 为基准（reload 触发的 loadFinished 不重置）
    m_loadStart.start();
    m_cycleStartMs = 0;
    m_reloaded = false;
    m_webView->load(url);
}

void HomePage::shutdown()
{
    if (m_readyTimer) {
        m_readyTimer->stop();
        m_readyTimer->deleteLater();
        m_readyTimer = nullptr;
    }
    if (!m_webView)
        return;
    m_webView->stop();
    m_webView->deleteLater(); // 事件循环中安全删除
    m_webView = nullptr;
}

} // namespace dshinqt
