#include <QtTest>

#include "git/gitclient.h"
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

QTEST_MAIN(TestCore)

#include "tst_core.moc"
