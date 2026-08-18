#include "preflightchecker.h"

#include "settings/appsettings.h"

namespace dshinqt {

PreflightChecker::PreflightChecker(QObject *parent)
    : QObject(parent)
{}

QList<CheckItem> PreflightChecker::check(const AppSettings &settings)
{
    return probeDependencies(settings.sourcePath);
}

} // namespace dshinqt
