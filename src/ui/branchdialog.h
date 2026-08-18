#pragma once

#include <QDialog>

class GitClient;
class QTreeWidget;

class BranchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BranchDialog(GitClient *git, bool selectOnly = false, QWidget *parent = nullptr);

    QString selectedBranch() const;

private slots:
    void onCheckout();

private:
    GitClient *m_git = nullptr;
    QTreeWidget *m_tree = nullptr;
    bool m_selectOnly = false;
};
