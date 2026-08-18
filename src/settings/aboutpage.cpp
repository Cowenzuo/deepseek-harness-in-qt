#include "aboutpage.h"

#include <QLabel>
#include <QVBoxLayout>

namespace dshinqt {

AboutPage::AboutPage(QWidget *parent)
    : QWidget(parent)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(28, 24, 28, 24);
    v->setSpacing(14);

    auto *name = new QLabel(QStringLiteral("deepseek-harness-in-qt"), this);
    name->setStyleSheet(QStringLiteral("color:#f5f5f5; font-size:20px; font-weight:700;"));

    // 软件定位
    auto *posTitle = new QLabel(QStringLiteral("软件定位"), this);
    posTitle->setObjectName(QStringLiteral("pageTitle"));
    auto *pos = new QLabel(QStringLiteral("deepseek-harness-in-qt 是 deepseek-harness（dsh）的桌面化管理工具。"
                                          "它把 dsh 从命令行变成常驻后台服务：用图形界面完成环境检测、更新"
                                          "与服务管理，内置 Web 界面，无需记忆任何命令。"),
                           this);
    pos->setWordWrap(true);
    pos->setStyleSheet(QStringLiteral("color:#c8c8c8; font-size:13px;"));

    // 主要特性
    auto *featTitle = new QLabel(QStringLiteral("主要特性"), this);
    featTitle->setObjectName(QStringLiteral("pageTitle"));
    auto *feats = new QLabel(QStringLiteral("• 环境自动检测：Node.js / pnpm / git 一键校验\n"
                                            "• 更新一目了然：与上游的领先 / 落后状态清晰可见\n"
                                            "• 更新管理：切换分支、切换提交、更新当前分支\n"
                                            "• 服务常驻后台：关闭窗口不影响 dsh 继续运行\n"
                                            "• 内置 Web UI：以浏览器内核渲染 dsh 界面"),
                             this);
    feats->setWordWrap(true);
    feats->setStyleSheet(QStringLiteral("color:#9a9a9a; font-size:13px;"));

    auto *repo = new QLabel(QStringLiteral("上游仓库：https://github.com/deepseek-ai/deepseek-harness"), this);
    repo->setStyleSheet(QStringLiteral("color:#7ab0ff; font-size:12px;"));
    auto *hint = new QLabel(QStringLiteral("dsh 作为常驻后台服务运行，关闭本窗口不影响其继续运行。"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#787878; font-size:12px;"));

    v->addWidget(name);
    v->addSpacing(6);
    v->addWidget(posTitle);
    v->addWidget(pos);
    v->addSpacing(8);
    v->addWidget(featTitle);
    v->addWidget(feats);
    v->addStretch(1);
    v->addWidget(repo);
    v->addWidget(hint);
}

} // namespace dshinqt
