#include "preflightchecker.h"

#include <QDir>
#include <QDirIterator>

#include "settings/appsettings.h"

namespace dshinqt {

PreflightChecker::PreflightChecker(QObject *parent)
    : QObject(parent)
{}

QList<CheckItem> PreflightChecker::check(const AppSettings &settings)
{
    QList<CheckItem> items;

    {
        CheckItem it;
        it.name = QStringLiteral("依赖已安装");
        it.passed = QDir(settings.sourcePath + QStringLiteral("/node_modules")).exists();
        it.detail =
            it.passed ? QStringLiteral("node_modules 存在") : QStringLiteral("缺少 node_modules，需 pnpm install");
        items.append(it);
    }

    {
        CheckItem it;
        it.name = QStringLiteral("Web 前端产物");
        it.passed = QDir(settings.sourcePath + QStringLiteral("/apps/web/dist")).exists();
        it.detail =
            it.passed ? QStringLiteral("apps/web/dist 存在") : QStringLiteral("缺少 apps/web/dist，需 pnpm run build");
        items.append(it);
    }

    // 库构建产物（packages 下任一非空 lib）
    {
        CheckItem it;
        it.name = QStringLiteral("库构建产物");
        bool found = false;
        QDirIterator itr(settings.sourcePath + QStringLiteral("/packages"),
                         QStringList() << QStringLiteral("lib"),
                         QDir::Dirs | QDir::NoDotAndDotDot,
                         QDirIterator::Subdirectories);
        while (itr.hasNext()) {
            itr.next();
            if (!QDir(itr.filePath()).entryList(QDir::Files).isEmpty()) {
                found = true;
                break;
            }
        }
        it.passed = found;
        it.detail =
            found ? QStringLiteral("packages 下存在 lib 产物") : QStringLiteral("缺少库构建产物，需 pnpm run build");
        items.append(it);
    }

    return items;
}

} // namespace dshinqt
