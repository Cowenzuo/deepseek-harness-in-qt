#pragma once

#include <QList>
#include <QString>

namespace dshinqt {

// 单项依赖/产物检查结果
struct CheckItem
{
    QString name;
    bool passed = false;
    QString detail;
};

// 依赖与构建产物体检（单一数据源，供 PreflightChecker 与 EnvironmentChecker 复用）：
// 1) node_modules 存在  2) apps/web/dist 存在  3) packages 下任一非空 lib
QList<CheckItem> probeDependencies(const QString &sourcePath);

} // namespace dshinqt
