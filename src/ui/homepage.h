#pragma once

#include <QElapsedTimer>
#include <QUrl>
#include <QWidget>

class QTimer;
class QWebEngineView;

namespace dshinqt {

// 主页：只承载 Web UI 的 webview（深色背景）。
// 在窗口 show 前同步创建 webview（顶层窗口第一帧即 RHI 形态，避免光栅→RHI 切换白闪），
// 与 stackwidget 平级叠放、常驻渲染（lower 被盖住），切页由 MainWindow 用 raise/lower 控制。
// 就绪判定：loadFinished 后轮询“正面锚点”（输入区/会话区 DOM 挂载），命中才发 pageReady，
// 由 MainWindow 决定显示——避免用户看到 dsh 早期加载的 “failed to load plugins” 警告。
class HomePage : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(QWidget *parent = nullptr);

    void load(const QUrl &url);
    void shutdown(); // 退出前释放 webview，服务常驻不受影响

signals:
    void pageReady();  // 正面锚点命中：输入区/会话区已挂载，界面可用
    void pageFailed(); // 总超时仍未见锚点，判定页面加载失败

private:
    void ensureWebView();   // 创建并初始化 webview（窗口显示前调用）
    void startReadyCheck(); // loadFinished 后启动锚点轮询
    void checkReadyOnce();  // 单次 runJavaScript 锚点查询

    QWebEngineView *m_webView = nullptr;
    QTimer *m_readyTimer = nullptr;
    QElapsedTimer m_loadStart; // 本次 load 起点（总超时基准）
    qint64 m_cycleStartMs = 0; // 当前周期（loadFinished/reload）起点
    bool m_reloaded = false;   // 本次加载是否已自愈 reload
};

} // namespace dshinqt
