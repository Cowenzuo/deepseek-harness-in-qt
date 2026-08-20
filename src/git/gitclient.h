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

// 提交行解析（可测纯函数）：输入 git log --pretty=format:%H%x09%an%x09%ad%x09%s 的输出，按行解析。
inline QList<GitCommit> parseGitLog(const QString &out)
{
    QList<GitCommit> result;
    const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList parts = line.split(QLatin1Char('\t'));
        if (parts.size() < 4)
            continue;
        GitCommit c;
        c.hash = parts[0];
        c.author = parts[1];
        c.date = parts[2];
        c.message = parts.mid(3).join(QLatin1Char('\t')); // 消息自身含 \t 时拼回
        result.append(c);
    }
    return result;
}

class GitClient : public QObject
{
    Q_OBJECT

public:
    explicit GitClient(const AppSettings *settings, QObject *parent = nullptr);

    // cwd：可选仓库工作目录；为空时使用 settings.sourcePath。
    // 后台线程调用时应显式传入快照副本，避免跨线程访问 AppSettings。
    QString currentBranch(const QString &cwd = {});
    bool isDirty(const QString &cwd = {});
    QList<GitBranch> branches(const QString &cwd = {});
    QList<GitCommit> commits(int limit, int offset, const QString &cwd = {});
    QList<GitCommit> commits(const QString &rev, int limit, int offset, const QString &cwd = {}); // rev 为空时表示 HEAD
    QList<GitCommit> searchCommits(const QString &keyword, int limit = 200, const QString &cwd = {});
    // 与上游相比的领先/落后提交数；无上游或失败返回 false
    bool aheadBehind(int *ahead, int *behind, const QString &cwd = {});
    // HEAD 提交的短哈希与提交时间（Unix 秒）；非 git 仓库/命令失败返回 false
    bool headCommit(QString *hash7, qint64 *epochSec, const QString &cwd = {});
    bool fetch(QString *errorOut = nullptr, const QString &cwd = {});
    bool checkoutBranch(const QString &name, QString *errorOut = nullptr, const QString &cwd = {});
    bool checkoutCommit(const QString &hash, QString *errorOut = nullptr, const QString &cwd = {});

private:
    // 返回标准输出；失败时 errorOut 非空
    QString run(const QStringList &args, const QString &cwd, QString *errorOut = nullptr, int timeoutMs = 20000);

    const AppSettings *m_settings = nullptr;
};

} // namespace dshinqt
