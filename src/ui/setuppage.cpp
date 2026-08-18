#include "setuppage.h"

#include <QColor>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include "process/environmentchecker.h"
#include "settings/appsettings.h"

namespace dshinqt {

SetupPage::SetupPage(AppSettings *settings, EnvironmentChecker *env, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_env(env)
{
    // 深色背景用 palette（可靠），QSS 只用于子控件
    setAutoFillBackground(true);
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(18, 18, 18));
        setPalette(pal);
    }
    applyStyleSheet();

    auto *title = new QLabel(QStringLiteral("欢迎使用 deepseek-harness-in-qt"), this);
    title->setObjectName(QStringLiteral("title"));
    title->setAlignment(Qt::AlignCenter);

    auto *subtitle = new QLabel(QStringLiteral("首次运行，请补全运行环境路径"), this);
    subtitle->setObjectName(QStringLiteral("subtitle"));
    subtitle->setAlignment(Qt::AlignCenter);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("card"));
    card->setMaximumWidth(880);
    card->setMinimumWidth(620);
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(32, 28, 32, 28);
    cardLayout->setSpacing(22);

    m_sourceEdit = new QLineEdit(this);
    m_sourceEdit->setPlaceholderText(QStringLiteral("选择含 pnpm-workspace.yaml 的目录"));
    m_nodeEdit = new QLineEdit(this);
    m_nodeEdit->setPlaceholderText(QStringLiteral("留空表示使用 PATH 中的 Node.js"));
    m_pnpmEdit = new QLineEdit(this);
    m_pnpmEdit->setPlaceholderText(QStringLiteral("留空表示使用 PATH 中的 pnpm"));
    m_gitEdit = new QLineEdit(this);
    m_gitEdit->setPlaceholderText(QStringLiteral("留空表示使用 PATH 中的 git"));

    m_sourceStatus = new QLabel(this);
    m_depsStatus = new QLabel(this);
    m_nodeStatus = new QLabel(this);
    m_pnpmStatus = new QLabel(this);
    m_gitStatus = new QLabel(this);

    auto *browseSource = new QPushButton(QStringLiteral("浏览..."), this);
    auto *browseNode = new QPushButton(QStringLiteral("浏览..."), this);
    auto *browsePnpm = new QPushButton(QStringLiteral("浏览..."), this);
    auto *browseGit = new QPushButton(QStringLiteral("浏览..."), this);

    cardLayout->addWidget(
        makeField(QStringLiteral("deepseek-harness 仓库路径"), m_sourceEdit, browseSource, m_sourceStatus));

    // 项目完整性（无输入框，跟随源码路径展示依赖/产物状态）
    {
        auto *depsBox = new QWidget(this);
        auto *depsV = new QVBoxLayout(depsBox);
        depsV->setContentsMargins(0, 0, 0, 0);
        depsV->setSpacing(6);
        auto *depsTitle = new QLabel(QStringLiteral("项目完整性"), depsBox);
        depsTitle->setObjectName(QStringLiteral("fieldTitle"));
        m_depsStatus->setTextFormat(Qt::RichText);
        depsV->addWidget(depsTitle);
        depsV->addWidget(m_depsStatus);
        cardLayout->addWidget(depsBox);
    }

    cardLayout->addWidget(makeField(QStringLiteral("Node.js 路径"), m_nodeEdit, browseNode, m_nodeStatus));
    cardLayout->addWidget(makeField(QStringLiteral("pnpm 路径"), m_pnpmEdit, browsePnpm, m_pnpmStatus));
    cardLayout->addWidget(makeField(QStringLiteral("git 路径"), m_gitEdit, browseGit, m_gitStatus));

    connect(browseSource, &QPushButton::clicked, this, &SetupPage::browseSource);
    connect(browseNode, &QPushButton::clicked, this, &SetupPage::browseNode);
    connect(browsePnpm, &QPushButton::clicked, this, &SetupPage::browsePnpm);
    connect(browseGit, &QPushButton::clicked, this, &SetupPage::browseGit);

    // 汇总提示 + 校验按钮
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setAlignment(Qt::AlignCenter);
    m_summaryLabel->setTextFormat(Qt::RichText);

    m_applyBtn = new QPushButton(QStringLiteral("校验"), this);
    m_applyBtn->setObjectName(QStringLiteral("primary"));
    m_applyBtn->setFocusPolicy(Qt::NoFocus); // 点击不抢焦点，避免禁用时焦点转移导致输入框全选
    connect(m_applyBtn, &QPushButton::clicked, this, &SetupPage::onApply);

    m_buildBtn = new QPushButton(QStringLiteral("一键构建依赖"), this);
    m_buildBtn->setObjectName(QStringLiteral("secondary"));
    m_buildBtn->setVisible(false);
    m_buildBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_buildBtn, &QPushButton::clicked, this, &SetupPage::buildRequested);

    m_cloneBtn = new QPushButton(QStringLiteral("克隆仓库"), this);
    m_cloneBtn->setObjectName(QStringLiteral("secondary"));
    m_cloneBtn->setVisible(false);
    m_cloneBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_cloneBtn, &QPushButton::clicked, this, &SetupPage::cloneRequested);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(48, 40, 48, 40);
    root->addStretch(1);
    root->addWidget(title);
    root->addSpacing(8);
    root->addWidget(subtitle);
    root->addSpacing(28);
    root->addWidget(card, 0, Qt::AlignHCenter);
    root->addSpacing(20);
    root->addWidget(m_summaryLabel);
    root->addSpacing(16);
    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch(1);
    buttonRow->addWidget(m_cloneBtn);
    buttonRow->addSpacing(12);
    buttonRow->addWidget(m_buildBtn);
    buttonRow->addSpacing(12);
    buttonRow->addWidget(m_applyBtn);
    buttonRow->addStretch(1);
    root->addLayout(buttonRow);
    root->addStretch(1);

    connect(m_env, &EnvironmentChecker::checkStarted, this, [this](int, const QString &name) { setChecking(name); });
    connect(m_env, &EnvironmentChecker::itemChecked, this, [this](int, const EnvItem &item) {
        updateField(item.name, item.passed, item.detail);
        if (item.name == QStringLiteral("deepseek-harness 仓库路径")) {
            m_sourceValid = item.passed;
            m_sourceCloneable = !item.passed && isDirCloneable(m_sourceEdit->text().trimmed());
        }
        if (!item.passed)
            m_allOk = false;
    });
    connect(m_env, &EnvironmentChecker::checkCompleted, this, [this]() { finishCheck(m_allOk); });

    prefill();
}

QWidget *SetupPage::makeField(const QString &title, QLineEdit *edit, QPushButton *browse, QLabel *status)
{
    auto *box = new QWidget(this);
    auto *v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);

    auto *titleLabel = new QLabel(title, box);
    titleLabel->setObjectName(QStringLiteral("fieldTitle"));

    browse->setObjectName(QStringLiteral("browse"));

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);
    row->addWidget(edit, 1);
    row->addWidget(browse);

    status->setTextFormat(Qt::RichText);

    v->addWidget(titleLabel);
    v->addLayout(row);
    v->addWidget(status);

    return box;
}

void SetupPage::setFieldStatus(QLabel *status, bool ok, const QString &text)
{
    if (ok)
        status->setText(QStringLiteral("<span style='color:#4caf50;'>✓ %1</span>").arg(text.toHtmlEscaped()));
    else
        status->setText(QStringLiteral("<span style='color:#e05f5f;'>✗ %1</span>").arg(text.toHtmlEscaped()));
}

void SetupPage::markInvalid(QLineEdit *edit, bool invalid)
{
    edit->setProperty("invalid", invalid);
    edit->style()->unpolish(edit);
    edit->style()->polish(edit);
}

void SetupPage::prefill()
{
    m_sourceEdit->setText(m_settings->sourcePath);
    m_nodeEdit->setText(m_settings->nodePath);
    m_pnpmEdit->setText(m_settings->pnpmPath);
    m_gitEdit->setText(m_settings->gitPath);

    const QString idle = QStringLiteral("<span style='color:#7a7a7a;'>未校验</span>");
    m_sourceStatus->setText(idle);
    m_depsStatus->setText(idle);
    m_nodeStatus->setText(idle);
    m_pnpmStatus->setText(idle);
    m_gitStatus->setText(idle);
    m_summaryLabel->setText(QStringLiteral("<span style='color:#9a9a9a;'>填写路径后点击「校验」开始校验</span>"));

    markInvalid(m_sourceEdit, false);
    markInvalid(m_nodeEdit, false);
    markInvalid(m_pnpmEdit, false);
    markInvalid(m_gitEdit, false);
    m_buildBtn->setVisible(false);
    m_cloneBtn->setVisible(false);
}

void SetupPage::browseSource()
{
    const QString dir =
        QFileDialog::getExistingDirectory(this, QStringLiteral("选择 deepseek-harness 源码目录"), m_sourceEdit->text());
    if (!dir.isEmpty())
        m_sourceEdit->setText(dir);
}

void SetupPage::browseNode()
{
    const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择 node 可执行文件"));
    if (!f.isEmpty())
        m_nodeEdit->setText(f);
}

void SetupPage::browsePnpm()
{
    const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择 pnpm 可执行文件"));
    if (!f.isEmpty())
        m_pnpmEdit->setText(f);
}

void SetupPage::browseGit()
{
    const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择 git 可执行文件"));
    if (!f.isEmpty())
        m_gitEdit->setText(f);
}

void SetupPage::onApply()
{
    // 先把输入框值写回 m_settings，克隆/构建流程（startClone/runBuildStep）读取的就是最新路径
    m_settings->sourcePath = QDir::cleanPath(QFileInfo(m_sourceEdit->text().trimmed()).absoluteFilePath());
    m_settings->nodePath = m_nodeEdit->text().trimmed();
    m_settings->pnpmPath = m_pnpmEdit->text().trimmed();
    m_settings->gitPath = m_gitEdit->text().trimmed();

    m_pendingSettings = *m_settings;

    const QString waiting = QStringLiteral("<span style='color:#7a7a7a;'>等待校验</span>");
    m_sourceStatus->setText(waiting);
    m_depsStatus->setText(waiting);
    m_nodeStatus->setText(waiting);
    m_pnpmStatus->setText(waiting);
    m_gitStatus->setText(waiting);
    markInvalid(m_sourceEdit, false);
    markInvalid(m_nodeEdit, false);
    markInvalid(m_pnpmEdit, false);
    markInvalid(m_gitEdit, false);

    m_allOk = true;
    m_sourceValid = false;
    m_sourceCloneable = false;
    m_buildBtn->setVisible(false);
    m_cloneBtn->setVisible(false);
    m_applyBtn->setEnabled(false);
    m_summaryLabel->setText(QStringLiteral("<span style='color:#9a9a9a;'>正在校验环境...</span>"));

    m_env->checkAsync(m_pendingSettings);
}

QLabel *SetupPage::statusFor(const QString &name) const
{
    if (name == QStringLiteral("deepseek-harness 仓库路径"))
        return m_sourceStatus;
    if (name == QStringLiteral("dsh 依赖"))
        return m_depsStatus;
    if (name == QStringLiteral("git"))
        return m_gitStatus;
    if (name == QStringLiteral("Node.js"))
        return m_nodeStatus;
    if (name == QStringLiteral("pnpm"))
        return m_pnpmStatus;
    return nullptr;
}

QLineEdit *SetupPage::editFor(const QString &name) const
{
    if (name == QStringLiteral("deepseek-harness 仓库路径"))
        return m_sourceEdit;
    if (name == QStringLiteral("git"))
        return m_gitEdit;
    if (name == QStringLiteral("Node.js"))
        return m_nodeEdit;
    if (name == QStringLiteral("pnpm"))
        return m_pnpmEdit;
    return nullptr;
}

void SetupPage::setChecking(const QString &name)
{
    if (QLabel *status = statusFor(name))
        status->setText(
            QStringLiteral("<span style='color:#d4a72c;'>⏳ 正在校验 %1 ...</span>").arg(name.toHtmlEscaped()));
}

void SetupPage::updateField(const QString &name, bool ok, const QString &detail)
{
    if (QLabel *status = statusFor(name))
        setFieldStatus(status, ok, detail);
    if (QLineEdit *edit = editFor(name))
        markInvalid(edit, !ok);
}

void SetupPage::finishCheck(bool allOk)
{
    if (allOk) {
        // 校验通过：保持按钮禁用，防止重复点击；停在引导页等待服务启动
        m_applyBtn->setEnabled(false);
        m_buildBtn->setVisible(false);
        m_cloneBtn->setVisible(false);
        m_summaryLabel->setText(QStringLiteral("<span style='color:#4caf50;'>✓ 校验通过，正在启动服务...</span>"));
        *m_settings = m_pendingSettings;
        emit finished();
    } else {
        // 校验失败：恢复按钮，允许修改后重新校验
        m_applyBtn->setEnabled(true);
        m_buildBtn->setVisible(m_sourceValid);

        const bool repoOk = isValidRepoUrl(m_settings->repoUrl);
        m_cloneBtn->setVisible(m_sourceCloneable && repoOk);

        if (m_sourceCloneable && !repoOk) {
            m_summaryLabel->setText(QStringLiteral(
                "<span style='color:#e05f5f;'>✗ 仓库地址无效，无法克隆，请在设置中配置正确的仓库地址</span>"));
        } else {
            m_summaryLabel->setText(
                QStringLiteral("<span style='color:#e05f5f;'>✗ 存在未通过的项，请修正后重试</span>"));
        }
        emit checkFailed();
    }
}

bool SetupPage::isValidRepoUrl(const QString &url)
{
    const QString u = url.trimmed();
    if (u.isEmpty())
        return false;
    return u.startsWith(QStringLiteral("http://")) || u.startsWith(QStringLiteral("https://")) ||
           u.startsWith(QStringLiteral("git@")) || u.startsWith(QStringLiteral("ssh://")) ||
           u.startsWith(QStringLiteral("git://")) || u.startsWith(QStringLiteral("file://"));
}

bool SetupPage::isDirCloneable(const QString &path) const
{
    const QString p = path.trimmed();
    if (p.isEmpty())
        return false;
    const QDir dir(p);
    if (!dir.exists())
        return true;                                                         // 目录不存在，git clone 会创建
    return dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty(); // 已存在但为空
}

void SetupPage::autoCheck()
{
    // 初始化自动校验：输入框已 prefill，直接复用校验流程
    onApply();
}

void SetupPage::recheck()
{
    onApply();
}

void SetupPage::applyStyleSheet()
{
    setStyleSheet(QStringLiteral(R"(
#title { color: #f5f5f5; font-size: 26px; font-weight: 700; }
#subtitle { color: #9a9a9a; font-size: 13px; }
#card { background: #1d1d1f; border: 1px solid #2b2b2e; border-radius: 16px; }
#fieldTitle { color: #b5b5b5; font-size: 12px; font-weight: 600; }
QLineEdit {
    background: #26262a; border: 1px solid #38383e; border-radius: 8px;
    padding: 10px 12px; color: #e8e8e8; font-size: 13px;
}
QLineEdit:focus { border: 1px solid #4f8cff; }
QLineEdit[invalid="true"] { border: 1px solid #e05f5f; }
#browse {
    background: #2c2c31; border: 1px solid #38383e; border-radius: 8px;
    padding: 10px 18px; color: #d2d2d6; font-size: 13px;
}
#browse:hover { background: #35353b; border-color: #4f8cff; color: #ffffff; }
#primary {
    background: #4f8cff; color: #ffffff; border: none; border-radius: 8px;
    padding: 12px 48px; font-size: 14px; font-weight: 700;
}
#primary:hover { background: #4077e0; }
#primary:pressed { background: #3566c4; }
#primary:disabled { background: #33333a; color: #777777; }
#secondary {
    background: #2c2c31; border: 1px solid #4f8cff; border-radius: 8px;
    padding: 11px 28px; color: #7ab0ff; font-size: 14px; font-weight: 600;
}
#secondary:hover { background: #35353b; color: #ffffff; }
)"));

} // namespace dshinqt
}
