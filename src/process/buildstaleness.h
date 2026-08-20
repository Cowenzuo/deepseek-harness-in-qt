#pragma once

#include <QDir>
#include <QFileInfo>
#include <QString>

namespace dshinqt {

// 构建产物（apps/web/dist）新鲜度判定，供启动前自检复用。
// 判据：HEAD 提交时间（秒）vs dist/index.html 修改时间（毫秒）——
// 产物早于最新提交（或缺失）视为过期。用提交时间而非哈希的原因：
// 外部手工 pnpm build 后产物晚于提交，不会误报；
// 自愈性：误判（时钟偏差）只会多触发一次重建，重建后产物必然新鲜。
inline qint64 distIndexMtimeMs(const QString &sourcePath)
{
    const QFileInfo fi(QDir(sourcePath).filePath(QStringLiteral("apps/web/dist/index.html")));
    return fi.exists() ? fi.lastModified().toMSecsSinceEpoch() : -1;
}

// 过期判定（可测纯函数）：产物缺失（-1）或早于 HEAD 提交时间 → true
inline bool isDistStale(qint64 headCommitEpochSec, qint64 distIndexMtimeMs)
{
    return distIndexMtimeMs < 0 || distIndexMtimeMs < headCommitEpochSec * 1000;
}

// 后台线程收集结果：commitEpochSec == -1 表示 git 不可用/非仓库（调用方应跳过检查）
struct BuildStalenessInfo
{
    qint64 commitEpochSec = -1; // HEAD 提交时间（Unix 秒）
    QString hash7;              // HEAD 短哈希（对话框展示）
    qint64 distMtimeMs = -1;    // dist/index.html 修改时间（毫秒）
};

} // namespace dshinqt
