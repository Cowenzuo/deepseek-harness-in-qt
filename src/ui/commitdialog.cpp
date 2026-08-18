#include "commitdialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

CommitDialog::CommitDialog(GitClient *git, QWidget *parent)
    : QDialog(parent)
    , m_git(git)
{
    setWindowTitle(QStringLiteral("提交记录"));
    resize(560, 520);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("按 hash 或提交消息搜索"));
    auto *searchBtn = new QPushButton(QStringLiteral("搜索"), this);
    auto *resetBtn = new QPushButton(QStringLiteral("重置"), this);

    m_list = new QListWidget(this);

    auto *top = new QHBoxLayout;
    top->addWidget(m_searchEdit, 1);
    top->addWidget(searchBtn);
    top->addWidget(resetBtn);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    QPushButton *selectBtn = buttons->addButton(QStringLiteral("选定"), QDialogButtonBox::AcceptRole);
    connect(selectBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_list, 1);
    layout->addWidget(buttons);

    connect(searchBtn, &QPushButton::clicked, this, &CommitDialog::onSearch);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &CommitDialog::onSearch);
    connect(resetBtn, &QPushButton::clicked, this, &CommitDialog::onReset);
    connect(m_list->verticalScrollBar(), &QScrollBar::valueChanged, this, &CommitDialog::onLoadMore);

    m_offset = 0;
    populate(m_git->commits(100, 0), true);
}

void CommitDialog::populate(const QList<GitCommit> &commits, bool replace)
{
    if (replace)
        m_list->clear();
    for (const auto &c : commits) {
        const QString text = QStringLiteral("%1  %2  %3\n    %4").arg(c.hash.left(10), c.date, c.author, c.message);
        auto *item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, c.hash);
        m_list->addItem(item);
    }
}

void CommitDialog::onSearch()
{
    const QString kw = m_searchEdit->text().trimmed();
    m_searchMode = !kw.isEmpty();
    populate(m_git->searchCommits(kw, 200), true);
}

void CommitDialog::onReset()
{
    m_searchEdit->clear();
    m_searchMode = false;
    m_offset = 0;
    populate(m_git->commits(100, 0), true);
}

void CommitDialog::onLoadMore(int value)
{
    Q_UNUSED(value);
    if (m_searchMode)
        return;
    QScrollBar *sb = m_list->verticalScrollBar();
    if (sb->value() >= sb->maximum() - 4) {
        m_offset += 100;
        populate(m_git->commits(100, m_offset), false);
    }
}

QString CommitDialog::selectedCommit() const
{
    auto *item = m_list->currentItem();
    if (!item)
        return {};
    return item->data(Qt::UserRole).toString();
}
