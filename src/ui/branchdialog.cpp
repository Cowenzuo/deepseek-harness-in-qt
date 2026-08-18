#include "branchdialog.h"

#include <QBrush>
#include <QColor>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "git/gitclient.h"

BranchDialog::BranchDialog(GitClient *git, bool selectOnly, QWidget *parent)
    : QDialog(parent)
    , m_git(git)
    , m_selectOnly(selectOnly)
{
    setWindowTitle(QStringLiteral("分支"));
    resize(360, 480);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);

    auto *localRoot = new QTreeWidgetItem(m_tree, {QStringLiteral("本地分支")});
    auto *remoteRoot = new QTreeWidgetItem(m_tree, {QStringLiteral("远程分支")});

    for (const auto &b : m_git->branches()) {
        auto *item = new QTreeWidgetItem({b.isCurrent ? b.name + QStringLiteral("（当前）") : b.name});
        item->setData(0, Qt::UserRole, b.name);
        if (b.isCurrent)
            item->setForeground(0, QBrush(QColor(0, 140, 0)));
        (b.isRemote ? remoteRoot : localRoot)->addChild(item);
    }
    m_tree->expandAll();

    auto *buttons = new QDialogButtonBox(this);
    QPushButton *actionBtn = buttons->addButton(m_selectOnly ? QStringLiteral("选定") : QStringLiteral("切换"),
                                                QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    connect(actionBtn, &QPushButton::clicked, this, &BranchDialog::onCheckout);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_tree);
    layout->addWidget(buttons);
}

QString BranchDialog::selectedBranch() const
{
    auto *item = m_tree->currentItem();
    if (!item || item->parent() == nullptr)
        return {};
    return item->data(0, Qt::UserRole).toString();
}

void BranchDialog::onCheckout()
{
    const QString name = selectedBranch();
    if (name.isEmpty())
        return;

    if (m_selectOnly) {
        accept(); // 仅返回选择
        return;
    }

    QString err;
    if (!m_git->checkoutBranch(name, &err)) {
        QMessageBox::warning(this, QStringLiteral("切换失败"), err);
        return;
    }
    QMessageBox::information(this, QStringLiteral("切换成功"), QStringLiteral("已切换到：%1").arg(name));
    accept();
}
