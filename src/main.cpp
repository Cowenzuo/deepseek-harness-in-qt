#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMutex>
#include <QPalette>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QStyleFactory>
#include <QTextStream>

#include "mainwindow.h"

// qDebug 落到 config/debug.log（WIN32 GUI 应用无控制台，需写文件才能观察时序）
static void debugMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(type);
    Q_UNUSED(context);
    static QFile file;
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    if (!file.isOpen()) {
        const QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/config");
        const bool mkOk = QDir().mkpath(dir);
        Q_UNUSED(mkOk);
        file.setFileName(dir + QStringLiteral("/debug.log"));
        const bool openOk = file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
        Q_UNUSED(openOk);
    }
    QTextStream ts(&file);
    ts << QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")) << ' ' << msg << '\n';
    ts.flush();
}

int main(int argc, char *argv[])
{
    // 须在 QApplication 创建前：
    // 1) 共享 OpenGL 上下文（QtWebEngine 官方推荐，配合 OpenGL 后端减少二次初始化闪烁）
    // 2) 强制统一 OpenGL 渲染后端：Windows 默认 D3D 合成与 Chromium 的 OpenGL 渲染
    //    切换会导致整窗消失/白闪（Qt Forum / GitHub 实证）。
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("deepseek-harness-in-qt"));
    app.setOrganizationName(QStringLiteral("deepseek-harness-in-qt"));
    app.setWindowIcon(QIcon(QStringLiteral(":/app.ico")));

    qInstallMessageHandler(debugMessageHandler);

    // 全局深色主题：统一窗口、菜单栏、状态栏等系统组件为深色，
    // 避免启动首帧/默认组件露出浅色（白闪）。
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    {
        QPalette pal;
        const QColor windowBg(18, 18, 18);
        const QColor widgetBg(30, 30, 34);
        pal.setColor(QPalette::Window, windowBg);
        pal.setColor(QPalette::Base, windowBg);
        pal.setColor(QPalette::AlternateBase, widgetBg);
        pal.setColor(QPalette::WindowText, QColor(220, 220, 220));
        pal.setColor(QPalette::Text, QColor(220, 220, 220));
        pal.setColor(QPalette::Button, widgetBg);
        pal.setColor(QPalette::ButtonText, QColor(220, 220, 220));
        pal.setColor(QPalette::ToolTipBase, widgetBg);
        pal.setColor(QPalette::ToolTipText, QColor(220, 220, 220));
        pal.setColor(QPalette::Highlight, QColor(66, 120, 200));
        pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        pal.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
        pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 120, 120));
        app.setPalette(pal);
    }

    qDebug() << "[main] MainWindow 构造前";
    MainWindow w;
    qDebug() << "[main] MainWindow 构造后";
    w.show();
    qDebug() << "[main] show() 后，进入事件循环";

    return app.exec();
}
