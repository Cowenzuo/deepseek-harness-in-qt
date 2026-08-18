#include "gitclient.h"

#include <QDate>
#include <QProcess>
#include <QRegularExpression>

#include "process/proxydetector.h"
#include "settings/appsettings.h"

namespace {
QList<GitCommit> parseLog(const QString &out)
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
        c.message = parts[3];
        result.append(c);
    }
    return result;
}
} // namespace

GitClient::GitClient(const AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{}

QString GitClient::run(const QStringList &args, QString *errorOut, int timeoutMs)
{
    QProcess p;
    p.setProgram(QStringLiteral("git"));

    // 自动注入系统代理（若已启用），让 fetch 等网络操作走代理
    QStringList fullArgs = ProxyDetector::gitProxyArgs();
    fullArgs << args;
    p.setArguments(fullArgs);

    p.setWorkingDirectory(m_settings->sourcePath);
    p.start();

    if (!p.waitForStarted(timeoutMs)) {
        if (errorOut)
            *errorOut = QStringLiteral("git 启动失败");
        return {};
    }
    if (!p.waitForFinished(timeoutMs)) {
        if (errorOut)
            *errorOut = QStringLiteral("git 执行超时");
        return {};
    }

    const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        if (errorOut)
            *errorOut = err;
        return {};
    }
    return QString::fromUtf8(p.readAllStandardOutput());
}

QString GitClient::currentBranch()
{
    return run({QStringLiteral("branch"), QStringLiteral("--show-current")}).trimmed();
}

bool GitClient::isDirty()
{
    QString err;
    const QString out = run({QStringLiteral("status"), QStringLiteral("--porcelain")}, &err);
    if (!err.isEmpty())
        return true; // 状态检查失败时按"脏"处理，避免误放行更新
    return !out.isEmpty();
}

QList<GitBranch> GitClient::branches()
{
    QList<GitBranch> result;
    const QString current = currentBranch();

    // 本地与远程分支（refs/heads / refs/remotes）
    {
        QString err;
        const QString out = run(
            {QStringLiteral("for-each-ref"), QStringLiteral("--format=%(refname:short)"), QStringLiteral("refs/heads")},
            &err);
        if (err.isEmpty()) {
            const QStringList names = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &n : names) {
                GitBranch b;
                b.name = n;
                b.isCurrent = (n == current);
                result.append(b);
            }
        }
    }
    {
        QString err;
        const QString out = run({QStringLiteral("for-each-ref"),
                                 QStringLiteral("--format=%(refname:short)"),
                                 QStringLiteral("refs/remotes")},
                                &err);
        if (err.isEmpty()) {
            const QStringList names = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &n : names) {
                GitBranch b;
                b.name = n;
                b.isRemote = true;
                result.append(b);
            }
        }
    }
    return result;
}

QList<GitCommit> GitClient::commits(int limit, int offset)
{
    return commits(QString(), limit, offset);
}

QList<GitCommit> GitClient::commits(const QString &rev, int limit, int offset)
{
    QString err;
    QStringList args = {QStringLiteral("log"),
                        QStringLiteral("--pretty=format:%H%x09%an%x09%ad%x09%s"),
                        QStringLiteral("--date=short")};
    if (!rev.isEmpty())
        args << rev;
    args << QStringLiteral("--skip=") + QString::number(offset) << QStringLiteral("-n") << QString::number(limit);
    const QString out = run(args, &err);
    if (!err.isEmpty())
        return {};
    return parseLog(out);
}

QList<GitCommit> GitClient::searchCommits(const QString &keyword, int limit)
{
    QString err;
    QStringList args = {QStringLiteral("log"),
                        QStringLiteral("--pretty=format:%H%x09%an%x09%ad%x09%s"),
                        QStringLiteral("--date=short")};

    static const QRegularExpression hexRe(QStringLiteral("^[0-9a-fA-F]{4,}$"));
    if (hexRe.match(keyword).hasMatch()) {
        args << keyword << QStringLiteral("-n") << QStringLiteral("1");
    } else {
        args << QStringLiteral("--all") << QStringLiteral("--grep=") + keyword << QStringLiteral("-n")
             << QString::number(limit);
    }

    const QString out = run(args, &err);
    if (!err.isEmpty())
        return {};
    return parseLog(out);
}

bool GitClient::aheadBehind(int *ahead, int *behind)
{
    QString err;
    const QString out = run({QStringLiteral("rev-list"),
                             QStringLiteral("--left-right"),
                             QStringLiteral("--count"),
                             QStringLiteral("HEAD...@{upstream}")},
                            &err);
    if (!err.isEmpty())
        return false; // 无上游分支 / 仓库无效
    const QStringList parts = out.trimmed().split(QRegularExpression(QStringLiteral("[\\s]+")), Qt::SkipEmptyParts);
    if (parts.size() < 2)
        return false;
    bool okA = false, okB = false;
    const int a = parts[0].toInt(&okA);
    const int b = parts[1].toInt(&okB);
    if (!okA || !okB)
        return false;
    if (ahead)
        *ahead = a;
    if (behind)
        *behind = b;
    return true;
}

bool GitClient::fetch(QString *errorOut)
{
    QString err;
    run({QStringLiteral("fetch"), QStringLiteral("--all"), QStringLiteral("--prune")}, &err, 120000);
    if (!err.isEmpty()) {
        if (errorOut)
            *errorOut = err;
        return false;
    }
    return true;
}

bool GitClient::checkoutBranch(const QString &name, QString *errorOut)
{
    QString target = name;
    const QString originPrefix = QStringLiteral("origin/");
    if (target.startsWith(originPrefix))
        target = target.mid(originPrefix.length());

    QString err;
    run({QStringLiteral("checkout"), target}, &err);
    if (!err.isEmpty()) {
        if (errorOut)
            *errorOut = err;
        return false;
    }
    return true;
}

bool GitClient::checkoutCommit(const QString &hash, QString *errorOut)
{
    // 基于该提交创建自动命名的新本地分支并检出（避免 detached HEAD，
    // 使切换后的版本可继续更新/开发）。同名分支已存在时直接检出该分支。
    const QString branchName =
        QStringLiteral("dsh/%1-%2").arg(QDate::currentDate().toString(QStringLiteral("yyyyMMdd")), hash.left(7));

    QString err;
    run({QStringLiteral("checkout"), branchName}, &err);
    if (err.isEmpty())
        return true; // 同名分支已存在，直接检出

    err.clear();
    run({QStringLiteral("checkout"), QStringLiteral("-b"), branchName, hash}, &err);
    if (!err.isEmpty()) {
        if (errorOut)
            *errorOut = err;
        return false;
    }
    return true;
}
