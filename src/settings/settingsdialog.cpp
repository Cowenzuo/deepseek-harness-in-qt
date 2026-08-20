#include "settingsdialog.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSize>
#include <QStackedWidget>

#include "aboutpage.h"
#include "appsettings.h"
#include "generalpage.h"
#include "git/gitclient.h"
#include "process/dshprocessmanager.h"
#include "repoupdatepage.h"
#include "servicepage.h"
#include "sessionrepairpage.h"
#include "update/updatemanager.h"

namespace dshinqt {

SettingsDialog::SettingsDialog(AppSettings *settings, GitClient *git, UpdateManager *update, DshProcessManager *proc,
                               QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("设置"));
    setModal(true);
    resize(900, 620);
    setMinimumSize(760, 560); // 更新页（分支卡 250px + 提交列表）在窄窗口下不挤压
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

    // 深色卡片式样式（与引导页一致）
    setStyleSheet(QStringLiteral(R"(
QDialog { background: #121212; }
QStackedWidget {
    background: #18181c; border: 1px solid #26262b; border-radius: 10px;
}
QListWidget#nav {
    background: #18181c; border: none; border-right: 1px solid #26262b;
    outline: none; padding: 16px 8px; font-size: 13px;
}
QListWidget#nav::item {
    color: #9a9aa0; background: transparent; border: none; border-radius: 8px;
    margin: 2px 4px; padding: 9px 10px;
}
QListWidget#nav::item:hover { background: #232329; color: #d6d6db; }
QListWidget#nav::item:selected {
    background: #2f3550; color: #ffffff;
    border-left: 3px solid #4f8cff; padding-left: 7px; /* 补偿指示条占位 */
}
QLabel#pageTitle { color: #f5f5f5; font-size: 15px; font-weight: 700; }
QLabel#fieldTitle { color: #b5b5b5; font-size: 12px; font-weight: 600; }
QLineEdit, QSpinBox, QComboBox {
    background: #26262a; border: 1px solid #38383e; border-radius: 8px;
    padding: 8px 12px; color: #e8e8e8; font-size: 13px;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border: 1px solid #4f8cff; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox QAbstractItemView {
    background: #1d1d1f; border: 1px solid #38383e; border-radius: 6px;
    color: #e8e8e8; selection-background-color: #2f3550; outline: none;
}
QFrame#card { background: #1d1d1f; border: 1px solid #2b2b2e; border-radius: 10px; }
QPushButton#primary {
    background: #4f8cff; color: #ffffff; border: none; border-radius: 8px;
    padding: 10px 28px; font-size: 13px; font-weight: 700;
}
QPushButton#primary:hover { background: #4077e0; }
QPushButton#primary:pressed { background: #3566c4; }
QPushButton#primary:disabled { background: #33333a; color: #777777; }
QPushButton#secondary {
    background: #2c2c31; border: 1px solid #4f8cff; border-radius: 8px;
    padding: 9px 18px; color: #7ab0ff; font-size: 13px; font-weight: 600;
}
QPushButton#secondary:hover { background: #35353b; color: #ffffff; }
QPushButton#secondary:pressed { background: #2f3550; }
QListWidget {
    background: #1d1d1f; border: 1px solid #2b2b2e; border-radius: 8px;
    color: #e8e8e8; font-size: 13px; padding: 6px; outline: none;
}
QListWidget::item { padding: 10px 12px; border-radius: 6px; }
QListWidget::item:hover { background: #26262a; }
QListWidget::item:selected { background: #2f3550; color: #ffffff; }
QTreeWidget {
    background: #1d1d1f; border: 1px solid #2b2b2e; border-radius: 8px;
    color: #e8e8e8; font-size: 13px; padding: 4px; outline: none;
}
QTreeWidget::item { padding: 6px 8px; border-radius: 6px; }
QTreeWidget::item:hover { background: #26262a; }
QTreeWidget::item:selected { background: #2f3550; color: #ffffff; }
QTreeWidget::branch { background: transparent; }
)"));

    // 左侧竖向导航（图标 + 横排文字，选中态左侧蓝条）
    m_nav = new QListWidget(this);
    m_nav->setObjectName(QStringLiteral("nav"));
    m_nav->setFixedWidth(116);
    m_nav->setIconSize(QSize(16, 16));
    m_nav->setFocusPolicy(Qt::WheelFocus);
    const struct
    {
        const char *text;
        const char *icon;
    } navItems[] = {
        {"常规", ":/icons/settings.svg"},
        {"服务", ":/icons/server.svg"},
        {"更新", ":/icons/refresh.svg"},
        {"修复", ":/icons/wrench.svg"},
        {"关于", ":/icons/info.svg"},
    };
    for (const auto &it : navItems) {
        auto *item = new QListWidgetItem(QIcon(QString::fromLatin1(it.icon)), QString::fromUtf8(it.text), m_nav);
        item->setSizeHint(QSize(100, 36)); // 图标+文字行高，紧凑不占满
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    connect(m_nav, &QListWidget::currentRowChanged, this, &SettingsDialog::onNavChanged);

    m_pages = new QStackedWidget(this);

    auto *generalPage = new GeneralSettingsPage(settings, proc, this);
    auto *servicePage = new ServiceSettingsPage(settings, proc, this);
    auto *repoPage = new RepoUpdatePage(settings, git, update, this);
    auto *repairPage = new SessionRepairPage(settings, this);
    auto *aboutPage = new AboutPage(this);
    m_pages->addWidget(generalPage);
    m_pages->addWidget(servicePage);
    m_pages->addWidget(repoPage);
    m_pages->addWidget(repairPage);
    m_pages->addWidget(aboutPage);

    // 页间协作：保存配置前等待更新页在途后台线程，保存成功后刷新仓库信息
    connect(generalPage, &GeneralSettingsPage::beforeSave, repoPage, &RepoUpdatePage::waitForBackgroundTasks);
    connect(generalPage, &GeneralSettingsPage::saved, repoPage, &RepoUpdatePage::refreshRepo);

    // 必须在 m_pages 建好之后再设当前行（否则 currentRowChanged 触发时页面容器为空）
    m_nav->setCurrentRow(0);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 20);
    layout->setSpacing(14);
    layout->addWidget(m_nav);
    layout->addWidget(m_pages, 1);
}

void SettingsDialog::onNavChanged(int row)
{
    if (row >= 0 && row < m_pages->count())
        m_pages->setCurrentIndex(row);
}

} // namespace dshinqt
