#pragma once

#include <QList>
#include <QObject>
#include <QString>

namespace dshinqt {

class AppSettings;

struct GitBranch
{
    QString name;
    bool isRemote = false;
    bool isCurrent = false;
};

struct GitCommit
{
    QString hash;
    QString author;
    QString date;
    QString message;
};

class GitClient : public QObject
{
    Q_OBJECT

public:
    explicit GitClient(const AppSettings *settings, QObject *parent = nullptr);

    QString currentBranch();
    bool isDirty();
    QList<GitBranch> branches();
    QList<GitCommit> commits(int limit, int offset);
    QList<GitCommit> commits(const QString &rev, int limit, int offset); // rev 为空时表示 HEAD
    QList<GitCommit> searchCommits(const QString &keyword, int limit = 200);
    // 与上游相比的领先/落后提交数；无上游或失败返回 false
    bool aheadBehind(int *ahead, int *behind);
    bool fetch(QString *errorOut = nullptr);
    bool checkoutBranch(const QString &name, QString *errorOut = nullptr);
    bool checkoutCommit(const QString &hash, QString *errorOut = nullptr);

private:
    // 返回标准输出；失败时 errorOut 非空
    QString run(const QStringList &args, QString *errorOut = nullptr, int timeoutMs = 20000);

    const AppSettings *m_settings = nullptr;
};

} // namespace dshinqt
