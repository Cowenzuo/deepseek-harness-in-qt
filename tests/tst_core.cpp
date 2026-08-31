#include <QtTest>

#include "git/gitclient.h"
#include "process/buildstaleness.h"
#include "process/dshprocessmanager.h"
#include "process/environmentchecker.h"
#include "process/proxydetector.h"
#include "process/readywaiter.h"
#include "settings/appsettings.h"

using namespace dshinqt;

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void toProxyUrl_data();
    void toProxyUrl();
    void parseGitLog_data();
    void parseGitLog();
    void nodeVersionAtLeast_data();
    void nodeVersionAtLeast();
    void dshBootInBody();
    void clampPort_data();
    void clampPort();
    void downloadDirectory();
    void isDistStale_data();
    void isDistStale();
    void extractWebToken_data();
    void extractWebToken();
};

void TestCore::toProxyUrl_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    QTest::newRow("empty") << QString() << QString();
    QTest::newRow("with-scheme") << QStringLiteral("http://127.0.0.1:7897") << QStringLiteral("http://127.0.0.1:7897");
    QTest::newRow("bare-host-port") << QStringLiteral("127.0.0.1:7897") << QStringLiteral("http://127.0.0.1:7897");
    QTest::newRow("spaces") << QStringLiteral("  127.0.0.1:7897  ") << QStringLiteral("http://127.0.0.1:7897");
}

void TestCore::toProxyUrl()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(ProxyDetector::toProxyUrl(input), expected);
}

void TestCore::parseGitLog_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<int>("count");
    QTest::addColumn<QString>("firstMessage");
    QTest::newRow("empty") << QString() << 0 << QString();
    QTest::newRow("normal") << QStringLiteral("abc123\tauthor\t2026-01-01\thello world\n")
                            << 1 << QStringLiteral("hello world");
    QTest::newRow("tab-in-message")
        << QStringLiteral("abc123\tauthor\t2026-01-01\thello\tworld\n") << 1 << QStringLiteral("hello\tworld");
    QTest::newRow("short-line") << QStringLiteral("abc123\tauthor\n") << 0 << QString();
}

void TestCore::parseGitLog()
{
    QFETCH(QString, input);
    QFETCH(int, count);
    QFETCH(QString, firstMessage);
    const QList<GitCommit> commits = dshinqt::parseGitLog(input);
    QCOMPARE(commits.size(), count);
    if (count > 0)
        QCOMPARE(commits.first().message, firstMessage);
}

void TestCore::nodeVersionAtLeast_data()
{
    QTest::addColumn<QString>("ver");
    QTest::addColumn<bool>("expected");
    QTest::newRow("v22.19.0") << QStringLiteral("v22.19.0") << true;
    QTest::newRow("v22.18.9") << QStringLiteral("v22.18.9") << false;
    QTest::newRow("v23.0.0") << QStringLiteral("v23.0.0") << true;
    QTest::newRow("no-v-prefix") << QStringLiteral("22.20.1") << true;
    QTest::newRow("garbage") << QStringLiteral("not a version") << false;
}

void TestCore::nodeVersionAtLeast()
{
    QFETCH(QString, ver);
    QFETCH(bool, expected);
    QCOMPARE(dshinqt::nodeVersionAtLeast(ver, 22, 19), expected);
}

void TestCore::dshBootInBody()
{
    QVERIFY(dshinqt::dshBootInBody(QByteArrayLiteral("<script>window.__DSH_BOOT__={}</script>")));
    QVERIFY(!dshinqt::dshBootInBody(QByteArrayLiteral("<html>loading...</html>")));
    QVERIFY(!dshinqt::dshBootInBody(QByteArray()));
}

void TestCore::clampPort_data()
{
    QTest::addColumn<int>("port");
    QTest::addColumn<int>("expected");
    QTest::newRow("normal") << 3081 << 3081;
    QTest::newRow("zero") << 0 << 3080;
    QTest::newRow("negative") << -5 << 3080;
    QTest::newRow("overflow") << 65536 << 3080;
    QTest::newRow("max") << 65535 << 65535;
}

void TestCore::clampPort()
{
    QFETCH(int, port);
    QFETCH(int, expected);
    QCOMPARE(AppSettings::clampPort(port), expected);
}

void TestCore::downloadDirectory()
{
    AppSettings s;
    // 未配置 → 回退系统下载目录（非空）
    s.downloadPath.clear();
    const QString fallback = s.downloadDirectory();
    QVERIFY(!fallback.isEmpty());
    QCOMPARE(fallback, AppSettings::defaultDownloadDirectory());
    // 已配置 → 使用配置值（去除首尾空白）
    s.downloadPath = QStringLiteral("D:/tmp/dl");
    QCOMPARE(s.downloadDirectory(), QStringLiteral("D:/tmp/dl"));
    s.downloadPath = QStringLiteral("  D:/tmp/dl2  ");
    QCOMPARE(s.downloadDirectory(), QStringLiteral("D:/tmp/dl2"));
}

void TestCore::isDistStale_data()
{
    QTest::addColumn<qint64>("commitSec");
    QTest::addColumn<qint64>("mtimeMs");
    QTest::addColumn<bool>("expected");
    // 产物缺失 → 过期
    QTest::newRow("missing") << qint64(1000) << qint64(-1) << true;
    // 产物早于提交 → 过期
    QTest::newRow("older-than-commit") << qint64(2000) << qint64(1000 * 1000) << true;
    // 产物与提交同时 → 新鲜（边界）
    QTest::newRow("equal") << qint64(1000) << qint64(1000 * 1000) << false;
    // 产物晚于提交 → 新鲜（含外部手工构建）
    QTest::newRow("newer-than-commit") << qint64(1000) << qint64(2000 * 1000) << false;
    QTest::newRow("fresh-after-day") << qint64(1787152310) << qint64(1787152310LL * 1000 + 3600 * 1000) << false;
}

void TestCore::isDistStale()
{
    QFETCH(qint64, commitSec);
    QFETCH(qint64, mtimeMs);
    QFETCH(bool, expected);
    // 限定命名空间：成员函数与自由函数同名，避免类内查找遮蔽
    QCOMPARE(dshinqt::isDistStale(commitSec, mtimeMs), expected);
}

void TestCore::extractWebToken_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    QTest::newRow("plain")
        << QStringLiteral("dsh web: http://127.0.0.1:3081/?token=dyFiUTSpIZ5YY9binWcx8AQFsa1bG1Y40BQTkqQzct0")
        << QStringLiteral("dyFiUTSpIZ5YY9binWcx8AQFsa1bG1Y40BQTkqQzct0");
    QTest::newRow("lan-suffix")
        << QStringLiteral("dsh web: http://127.0.0.1:4567/?token=test-token (LAN: http://192.168.1.5:4567/?token=test-token)")
        << QStringLiteral("test-token");
    QTest::newRow("multi-line")
        << QStringLiteral("已后台启动：node\ndsh web: http://127.0.0.1:3081/?token=abc123\nopening browser")
        << QStringLiteral("abc123");
    QTest::newRow("none") << QStringLiteral("some log without token") << QString();
    QTest::newRow("empty") << QString() << QString();
}

void TestCore::extractWebToken()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(DshProcessManager::extractWebToken(input), expected);
}

QTEST_MAIN(TestCore)

#include "tst_core.moc"
