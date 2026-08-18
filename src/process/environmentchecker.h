#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "settings/appsettings.h"

namespace dshinqt {

struct EnvItem
{
    QString name; // 显示名
    bool passed = false;
    QString detail; // 版本号或错误说明
};

// 启动时的环境检测：git / node / pnpm / deepseek-harness 仓库路径。
// 自动探测可回填 settings 中的空路径，检测失败项供引导页补全。
class EnvironmentChecker : public QObject
{
    Q_OBJECT

public:
    explicit EnvironmentChecker(QObject *parent = nullptr);

    // 回填 settings 中为空的环境路径字段（自动探测，不覆盖已有值）
    void autoDetect(AppSettings *settings) const;

    // 静态单项校验，供引导页复用
    static bool isSourceValid(const QString &sourcePath);
    static QString findProgram(const QString &name);

public slots:
    // 异步逐项校验（引导页用），通过信号实时反馈进度
    void checkAsync(const AppSettings &settings);

signals:
    void checkStarted(int index, const QString &name);
    void itemChecked(int index, const EnvItem &item);
    void checkCompleted();

private:
    EnvItem checkOne(const QString &name, const AppSettings &settings) const;
    void runNext();

    QString runCapture(const QString &program, const QStringList &args, int timeoutMs) const;
    static QString programFor(const QString &explicitPath, const QString &fallback);

    AppSettings m_asyncSettings;
    QStringList m_asyncNames;
    int m_asyncIndex = 0;
};

} // namespace dshinqt
