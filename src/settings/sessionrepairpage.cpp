#include "sessionrepairpage.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QVBoxLayout>

#include "process/commandrunner.h"
#include "settings/appsettings.h"

namespace dshinqt {

namespace {
QLabel *fieldTitle(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setObjectName(QStringLiteral("fieldTitle"));
    return l;
}
} // namespace

SessionRepairPage::SessionRepairPage(AppSettings *settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    m_runner = new CommandRunner(this);
    connect(m_runner, &CommandRunner::outputReady, this, &SessionRepairPage::onRunnerOutput);

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(28, 24, 28, 24);
    v->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("会话修复"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    v->addWidget(title);

    auto *hint = new QLabel(
        QStringLiteral("输入加载失败会话的 GUID（支持短前缀，如 9de9db4d）。\n"
                       "工具会备份后修复 seq 重复/断裂（崩溃恢复与陈旧写入方并发所致），"
                       "修复完成需刷新页面重新加载会话。"),
        this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#9a9aa0; font-size:12px;"));
    v->addWidget(hint);

    m_guidEdit = new QLineEdit(this);
    m_guidEdit->setPlaceholderText(QStringLiteral("会话 GUID，如 9de9db4d-4368-457f-a39f-2c02fda91a26"));
    v->addWidget(m_guidEdit);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(10);
    m_diagnoseBtn = new QPushButton(QStringLiteral("扫描诊断"), this);
    m_diagnoseBtn->setObjectName(QStringLiteral("secondary"));
    m_diagnoseBtn->setCursor(Qt::PointingHandCursor);
    m_repairBtn = new QPushButton(QStringLiteral("修复"), this);
    m_repairBtn->setObjectName(QStringLiteral("primary"));
    m_repairBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    m_cancelBtn->setObjectName(QStringLiteral("secondary"));
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setVisible(false);
    btnRow->addWidget(m_diagnoseBtn);
    btnRow->addWidget(m_repairBtn);
    btnRow->addWidget(m_cancelBtn);
    btnRow->addStretch(1);
    v->addLayout(btnRow);

    m_output = new QPlainTextEdit(this);
    m_output->setObjectName(QStringLiteral("repairOutput"));
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(4000); // 防爆
    m_output->setStyleSheet(QStringLiteral(R"(
QPlainTextEdit#repairOutput {
    background: #1d1d1f; border: 1px solid #2b2b2e; border-radius: 8px;
    color: #c8c8c8; font-family: Consolas, monospace; font-size: 12px;
    padding: 8px;
}
)"));
    v->addWidget(m_output, 1);

    connect(m_diagnoseBtn, &QPushButton::clicked, this, &SessionRepairPage::runDiagnose);
    connect(m_repairBtn, &QPushButton::clicked, this, &SessionRepairPage::runRepair);
    connect(m_cancelBtn, &QPushButton::clicked, this, [this] { m_runner->cancel(); });
}

QString SessionRepairPage::scriptPath() const
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/tools/session-repair.mts");
}

void SessionRepairPage::runDiagnose()
{
    runAction(QStringLiteral("diagnose"), QString(), QString());
}

void SessionRepairPage::runRepair()
{
    const QString guid = m_guidEdit->text().trimmed();
    runAction(
        QStringLiteral("repair"), QStringLiteral("确认修复"),
        QStringLiteral("将备份会话日志后修复 seq 异常（%1）。\n\n"
                       "修复过程中请勿同时使用该会话；若目标为当前活动会话，工具会拒绝。\n\n是否继续？")
            .arg(guid.isEmpty() ? QStringLiteral("GUID 未填时按目录搜索") : guid));
}

void SessionRepairPage::runAction(const QString &action, const QString &confirmTitle, const QString &confirmText)
{
    if (m_runner->isBusy())
        return;
    const QString guid = m_guidEdit->text().trimmed();
    if (guid.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("会话修复"), QStringLiteral("请输入会话 GUID。"));
        return;
    }
    if (!confirmTitle.isEmpty()) {
        if (QMessageBox::question(this, confirmTitle, confirmText, QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
    }
    if (!QFileInfo::exists(scriptPath())) {
        appendOutput(QStringLiteral("[错误] 修复脚本缺失：%1").arg(scriptPath()), true);
        return;
    }

    const QString node = m_settings->nodePath.isEmpty() ? QStringLiteral("node") : m_settings->nodePath;
    const QStringList args = {QStringLiteral("--import"), QStringLiteral("tsx/esm"), scriptPath(),
                              QStringLiteral("--repo"),   m_settings->sourcePath,
                              QStringLiteral("--guid"),   guid,
                              QStringLiteral("--action"), action};
    appendOutput(QStringLiteral("> %1 %2").arg(node, args.join(' ')));
    setBusy(true);
    m_runner->start(node, args, m_settings->sourcePath, QProcessEnvironment(),
                    [this](bool success, int code) { onRunnerDone(success, code); });
}

void SessionRepairPage::onRunnerOutput(const QString &text)
{
    appendOutput(text);
}

void SessionRepairPage::onRunnerDone(bool success, int code)
{
    setBusy(false);
    appendOutput(success ? QStringLiteral("[完成] 退出码 0")
                         : QStringLiteral("[完成] 失败（code=%1）").arg(code),
                 !success);
}

void SessionRepairPage::appendOutput(const QString &line, bool isError)
{
    QTextCursor cur = m_output->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertText(line.endsWith(QLatin1Char('\n')) ? line : line + QLatin1Char('\n'));
    m_output->setTextCursor(cur);
    m_output->ensureCursorVisible();
    Q_UNUSED(isError); // 输出区统一配色，错误行已由脚本前缀标记
}

void SessionRepairPage::setBusy(bool busy)
{
    m_diagnoseBtn->setEnabled(!busy);
    m_repairBtn->setEnabled(!busy);
    m_cancelBtn->setVisible(busy);
    m_guidEdit->setEnabled(!busy);
}

} // namespace dshinqt
