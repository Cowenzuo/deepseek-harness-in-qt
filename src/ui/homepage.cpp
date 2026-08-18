#include "homepage.h"

#include <QBoxLayout>
#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QPalette>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>
#include <QWebEnginePage>
#include <QWebEngineView>

namespace {
const QColor kBg(18, 18, 18);

// 正面就绪锚点：输入区(data-composer-card) + 会话区(data-conversation-scroll) 均挂载
// = 聊天界面真正可用。只判“该出现的东西出现没有”，不枚举任何 warning。
const char kJsReadyCheck[] = "!!document.querySelector('[data-composer-card]')"
                             " && !!document.querySelector('[data-conversation-scroll]')";

const int kCheckIntervalMs = 500;  // 轮询间隔（事件驱动反压，非固定延时）
const int kReloadAfterMs = 10000;  // 单周期未命中 → 自愈 reload（对应 reload 能自愈的现象）
const int kTotalTimeoutMs = 60000; // 总超时 → pageFailed
} // namespace

HomePage::HomePage(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, kBg);
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
    m_webView->page()->setBackgroundColor(kBg);
    m_webView->setAutoFillBackground(true);
    {
        QPalette webPal = m_webView->palette();
        webPal.setColor(QPalette::Window, kBg);
        webPal.setColor(QPalette::Base, kBg);
        m_webView->setPalette(webPal);
    }
    m_webView->setStyleSheet(QStringLiteral("QWebEngineView { background: #121212; }"));

    if (auto *box = qobject_cast<QBoxLayout *>(layout()))
        box->addWidget(m_webView);
    // 每次加载完成：先抓取渲染后的完整 body 保存到 config/webview-N.html 供对比（调试），
    // 再启动正面锚点轮询，命中（输入区+会话区挂载）才认为界面就绪。
    connect(m_webView, &QWebEngineView::loadFinished, this, [this](bool ok) {
        qDebug() << "[UI] webview loadFinished ok=" << ok;
        m_webView->page()->toHtml([this](const QString &html) {
            static int sN = 0;
            ++sN;
            const QString path =
                QCoreApplication::applicationDirPath() + QStringLiteral("/config/webview-%1.html").arg(sN);
            QFile f(path);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(html.toUtf8());
                f.close();
            }
            qDebug() << "[UI] webview body len=" << html.size() << "saved=" << path;
        });
        // 重置就绪检查状态，开始新一轮锚点轮询
        m_loadStart.start();
        m_cycleStartMs = 0;
        m_reloaded = false;
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
        // 未命中且未到动作点：继续轮询（事件驱动）
    });
}

void HomePage::load(const QUrl &url)
{
    ensureWebView();
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
