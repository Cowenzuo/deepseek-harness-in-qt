#pragma once

#include <QWidget>

namespace dshinqt {

// 设置弹窗「关于」页：软件定位与特性说明（纯静态）。
class AboutPage : public QWidget
{
    Q_OBJECT

public:
    explicit AboutPage(QWidget *parent = nullptr);
};

} // namespace dshinqt
