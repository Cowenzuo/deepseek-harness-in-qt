#include "generalpage.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "appsettings.h"
#include "process/dshprocessmanager.h"

namespace dshinqt {

namespace {
QLabel *fieldTitle(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("fieldTitle"));
    return l;
}
} // namespace

GeneralSettingsPage::GeneralSettingsPage(AppSettings *settings, DshProcessManager *proc, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_proc(proc)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(28, 24, 28, 24);
    v->setSpacing(18);

    m_sourcePathEdit = new QLineEdit(m_settings->sourcePath, this);
    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(m_settings->webPort);
    m_nodePathEdit = new QLineEdit(m_settings->nodePath, this);
    m_nodePathEdit->setPlaceholderText(QStringLiteral("留空表示使用 PATH 中的 Node.js"));
    m_pnpmPathEdit = new QLineEdit(m_settings->pnpmPath, this);
    m_pnpmPathEdit->setPlaceholderText(QStringLiteral("留空表示使用 PATH 中的 pnpm"));
    m_gitPathEdit = new QLineEdit(m_settings->gitPath, this);
    m_gitPathEdit->setPlaceholderText(QStringLiteral("留空表示使用 PATH 中的 git"));
    m_repoUrlEdit = new QLineEdit(m_settings->repoUrl, this);
    m_downloadPathEdit = new QLineEdit(m_settings->downloadPath, this);
    m_downloadPathEdit->setPlaceholderText(QStringLiteral("留空表示使用系统下载目录"));
    auto *downloadBrowseBtn = new QPushButton(QStringLiteral("浏览..."), this);
    downloadBrowseBtn->setCursor(Qt::PointingHandCursor);
    downloadBrowseBtn->setFocusPolicy(Qt::NoFocus);
    // 与 QLineEdit 相同的 padding/border/圆角，保证与输入框等高等视觉对齐
    // （弹窗 QSS 只样式化了 QLineEdit，未样式化的按钮是 Fusion 默认矮高度）
    downloadBrowseBtn->setStyleSheet(QStringLiteral(R"(
QPushButton {
    background: #2c2c31; border: 1px solid #38383e; border-radius: 8px;
    padding: 8px 12px; color: #d6d6db; font-size: 13px;
}
QPushButton:hover { background: #35353b; border-color: #4f8cff; color: #ffffff; }
QPushButton:pressed { background: #2f3550; }
)"));
    connect(downloadBrowseBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, QStringLiteral("选择下载目录"),
            m_downloadPathEdit->text().trimmed().isEmpty() ? AppSettings::defaultDownloadDirectory()
                                                           : m_downloadPathEdit->text().trimmed());
        if (!dir.isEmpty())
            m_downloadPathEdit->setText(QDir::toNativeSeparators(dir));
    });

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(14);
    grid->setColumnStretch(1, 1);
    int r = 0;
    grid->addWidget(fieldTitle(QStringLiteral("deepseek-harness 仓库路径"), this), r, 0);
    grid->addWidget(m_sourcePathEdit, r++, 1);
    grid->addWidget(fieldTitle(QStringLiteral("Web UI 端口"), this), r, 0);
    grid->addWidget(m_portSpin, r++, 1);
    grid->addWidget(fieldTitle(QStringLiteral("Node.js 路径"), this), r, 0);
    grid->addWidget(m_nodePathEdit, r++, 1);
    grid->addWidget(fieldTitle(QStringLiteral("pnpm 路径"), this), r, 0);
    grid->addWidget(m_pnpmPathEdit, r++, 1);
    grid->addWidget(fieldTitle(QStringLiteral("git 路径"), this), r, 0);
    grid->addWidget(m_gitPathEdit, r++, 1);
    grid->addWidget(fieldTitle(QStringLiteral("仓库地址"), this), r, 0);
    grid->addWidget(m_repoUrlEdit, r++, 1);
    grid->addWidget(fieldTitle(QStringLiteral("下载目录"), this), r, 0);
    {
        auto *box = new QHBoxLayout;
        box->setSpacing(8);
        box->addWidget(m_downloadPathEdit, 1);
        box->addWidget(downloadBrowseBtn);
        grid->addLayout(box, r++, 1);
    }

    auto *saveBtn = new QPushButton(QStringLiteral("保存配置"), this);
    saveBtn->setObjectName(QStringLiteral("primary"));
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setFocusPolicy(Qt::NoFocus);
    connect(saveBtn, &QPushButton::clicked, this, &GeneralSettingsPage::saveSettings);

    auto *row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(saveBtn);

    v->addLayout(grid);
    v->addStretch(1); // 保存按钮沉底
    v->addLayout(row);
}

void GeneralSettingsPage::saveSettings()
{
    const QString path = m_sourcePathEdit->text().trimmed();
    if (!QFileInfo::exists(path + QStringLiteral("/pnpm-workspace.yaml"))) {
        QMessageBox::warning(this, QStringLiteral("设置"), QStringLiteral("源码路径无效：未找到 pnpm-workspace.yaml"));
        return;
    }
    // 保存前等待在途后台 git 线程（由壳转发到更新页）
    emit beforeSave();

    const bool svcRunning = m_proc->isRunning();
    const bool svcChanged = (path != m_settings->sourcePath) || (m_portSpin->value() != m_settings->webPort);

    // 源码路径统一转绝对路径（GUI 应用 cwd 不稳定，避免相对路径解析漂移）
    m_settings->sourcePath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    m_settings->webPort = m_portSpin->value();
    m_settings->nodePath = m_nodePathEdit->text().trimmed();
    m_settings->pnpmPath = m_pnpmPathEdit->text().trimmed();
    m_settings->gitPath = m_gitPathEdit->text().trimmed();
    m_settings->repoUrl = m_repoUrlEdit->text().trimmed();
    m_settings->downloadPath = m_downloadPathEdit->text().trimmed();
    if (!m_settings->save()) {
        QMessageBox::warning(this, QStringLiteral("设置"), QStringLiteral("配置保存失败。"));
        return;
    }
    emit saved();
    if (svcRunning && svcChanged) {
        if (QMessageBox::question(this, QStringLiteral("重启服务"),
                                  QStringLiteral("端口/源码路径已变更，需重启 dsh 服务才能生效。是否立即重启？"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
            == QMessageBox::Yes) {
            m_proc->restart();
        }
    } else {
        QMessageBox::information(this, QStringLiteral("设置"), QStringLiteral("配置已保存。"));
    }
}

} // namespace dshinqt
