#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "process/dependencyprobe.h"

namespace dshinqt {

class AppSettings;

// 启动前体检：依赖与构建产物检查（环境项由 EnvironmentChecker 负责）。
// 检测逻辑统一收敛到 probeDependencies（dependencyprobe.h），避免双份维护。
class PreflightChecker : public QObject
{
    Q_OBJECT

public:
    explicit PreflightChecker(QObject *parent = nullptr);

    QList<CheckItem> check(const AppSettings &settings);
};

} // namespace dshinqt
