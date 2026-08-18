#pragma once

#include <QList>
#include <QObject>
#include <QString>

class AppSettings;

struct CheckItem
{
    QString name;
    bool passed = false;
    QString detail;
};

class PreflightChecker : public QObject
{
    Q_OBJECT

public:
    explicit PreflightChecker(QObject *parent = nullptr);

    // 依赖与构建产物体检（git/node/pnpm 等环境项由 EnvironmentChecker 负责）
    QList<CheckItem> check(const AppSettings &settings);
};
