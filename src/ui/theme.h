#pragma once

#include <QColor>
#include <QPalette>

namespace dshinqt {
namespace Theme {

// 全局深色主题色板（单一来源；各页面与 main 统一取用，避免色值散落）
inline QColor windowBg()
{
    return QColor(18, 18, 18); // #121212
}

inline QColor widgetBg()
{
    return QColor(30, 30, 34);
}

inline QColor accent()
{
    return QColor(66, 120, 200);
}

// 全局深色 palette（Fusion 风格统一窗口、菜单栏、状态栏等系统组件）
inline QPalette darkPalette()
{
    QPalette pal;
    pal.setColor(QPalette::Window, windowBg());
    pal.setColor(QPalette::Base, windowBg());
    pal.setColor(QPalette::AlternateBase, widgetBg());
    pal.setColor(QPalette::WindowText, QColor(220, 220, 220));
    pal.setColor(QPalette::Text, QColor(220, 220, 220));
    pal.setColor(QPalette::Button, widgetBg());
    pal.setColor(QPalette::ButtonText, QColor(220, 220, 220));
    pal.setColor(QPalette::ToolTipBase, widgetBg());
    pal.setColor(QPalette::ToolTipText, QColor(220, 220, 220));
    pal.setColor(QPalette::Highlight, accent());
    pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 120, 120));
    return pal;
}

} // namespace Theme
} // namespace dshinqt
