#pragma once

#include <QDialog>

#include "git/gitclient.h"

class GitClient;
class QLineEdit;
class QListWidget;

class CommitDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CommitDialog(GitClient *git, QWidget *parent = nullptr);

    QString selectedCommit() const;

private slots:
    void onSearch();
    void onReset();
    void onLoadMore(int value);

private:
    void populate(const QList<GitCommit> &commits, bool replace);

    GitClient *m_git = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QListWidget *m_list = nullptr;
    int m_offset = 0;
    bool m_searchMode = false;
};
