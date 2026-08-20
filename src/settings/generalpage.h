#pragma once

#include <QWidget>

class QLineEdit;
class QSpinBox;

namespace dshinqt {

class AppSettings;
class DshProcessManager;

// 设置弹窗「常规」页：配置表单与保存（校验、绝对路径、重启生效提示）。
class GeneralSettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit GeneralSettingsPage(AppSettings *settings, DshProcessManager *proc, QWidget *parent = nullptr);

signals:
    void beforeSave(); // 保存前钩子（同步执行：等待在途后台 git 线程）
    void saved();      // 保存成功后

private:
    void saveSettings();

    AppSettings *m_settings = nullptr;
    DshProcessManager *m_proc = nullptr;
    QLineEdit *m_sourcePathEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QLineEdit *m_nodePathEdit = nullptr;
    QLineEdit *m_pnpmPathEdit = nullptr;
    QLineEdit *m_gitPathEdit = nullptr;
    QLineEdit *m_repoUrlEdit = nullptr;
    QLineEdit *m_downloadPathEdit = nullptr;
};

} // namespace dshinqt
