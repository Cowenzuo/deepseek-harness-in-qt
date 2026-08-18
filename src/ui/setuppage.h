#pragma once

#include <QWidget>

#include "settings/appsettings.h"

class EnvironmentChecker;
class QLabel;
class QLineEdit;
class QPushButton;

// 引导页：环境检测失败时让用户补全缺失路径（深色卡片式单页表单）。
class SetupPage : public QWidget
{
    Q_OBJECT

public:
    explicit SetupPage(AppSettings *settings, EnvironmentChecker *env, QWidget *parent = nullptr);

    void prefill();   // 用当前 settings 预填输入框并重置状态
    void autoCheck(); // 初始化自动校验（prefill 后自动走校验流程）
    void recheck();   // 重新校验（一键构建完成后由外部调用）

signals:
    void finished();       // 校验全部通过，值已写回 settings
    void checkFailed();    // 校验未通过（初始化自动校验失败时切引导页）
    void buildRequested(); // 依赖缺失，请求一键构建
    void cloneRequested(); // 源码目录为空，请求克隆仓库

private slots:
    void browseSource();
    void browseNode();
    void browsePnpm();
    void browseGit();
    void onApply();

private:
    QWidget *makeField(const QString &title, QLineEdit *edit, QPushButton *browse, QLabel *status);
    void setFieldStatus(QLabel *status, bool ok, const QString &text);
    void markInvalid(QLineEdit *edit, bool invalid);
    void applyStyleSheet();
    QLabel *statusFor(const QString &name) const;
    QLineEdit *editFor(const QString &name) const;
    void updateField(const QString &name, bool ok, const QString &detail);
    void setChecking(const QString &name);
    void finishCheck(bool allOk);
    bool isDirCloneable(const QString &path) const;
    static bool isValidRepoUrl(const QString &url);

    AppSettings *m_settings = nullptr;
    EnvironmentChecker *m_env = nullptr;
    QLineEdit *m_sourceEdit = nullptr;
    QLineEdit *m_nodeEdit = nullptr;
    QLineEdit *m_pnpmEdit = nullptr;
    QLineEdit *m_gitEdit = nullptr;
    QLabel *m_sourceStatus = nullptr;
    QLabel *m_depsStatus = nullptr;
    QLabel *m_nodeStatus = nullptr;
    QLabel *m_pnpmStatus = nullptr;
    QLabel *m_gitStatus = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QPushButton *m_applyBtn = nullptr;
    QPushButton *m_buildBtn = nullptr;
    QPushButton *m_cloneBtn = nullptr;
    AppSettings m_pendingSettings;
    bool m_allOk = false;
    bool m_sourceValid = false;
    bool m_sourceCloneable = false;
};
