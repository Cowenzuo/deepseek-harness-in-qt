#pragma once

#include <QDialog>

class QListWidget;
class QStackedWidget;

namespace dshinqt {

class AppSettings;
class DshProcessManager;
class GeneralSettingsPage;
class GitClient;
class RepoUpdatePage;
class ServiceSettingsPage;
class UpdateManager;

// 统一设置弹窗：左侧竖向导航（常规 / 服务 / 更新 / 关于）+ 右侧页面。
// 各页实现已拆分为 GeneralSettingsPage / ServiceSettingsPage / RepoUpdatePage / AboutPage，
// 本类只做导航装配与页间信号转发。
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(AppSettings *settings, GitClient *git, UpdateManager *update, DshProcessManager *proc,
                            QWidget *parent = nullptr, int initialPage = 0);

private slots:
    void onNavChanged(int row); // 左侧导航切换页面

private:
    QListWidget *m_nav = nullptr;
    QStackedWidget *m_pages = nullptr;
    int m_initialPage = 0; // 打开时定位的导航页（状态栏「修复」入口直达修复页）
};

} // namespace dshinqt
