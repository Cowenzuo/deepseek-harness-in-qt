# Phase 4 — Git 查看

## 目标

实现 git 命令封装与分支/提交/状态查看界面。

## 前置条件

- Phase 1 完成（UI 骨架）；菜单集成可与 Phase 2 并行。

## 任务清单

- [ ] 编写 `src/git/gitclient.{h,cpp}`：QProcess 调系统 `git`，异步返回
  - `localAndRemoteBranches()`：本地 + 远程分支
  - `commits(limit, offset)`：当前分支 log（hash/作者/时间/消息）
  - `status()`：`git status` 摘要
  - `fetch()`：`git fetch --all --prune`
  - `checkoutBranch(name)` / `checkoutCommit(hash)`
  - `currentBranch()` / `isDirty()`（`git status --porcelain` 非空）
- [ ] 编写 `src/ui/branchdialog.{h,cpp}`：分支列表（本地+远程），当前分支高亮，支持切换（checkout）。
- [ ] 编写 `src/ui/commitdialog.{h,cpp}`：提交列表**动态加载**（滚动到底再拉一批约 100 条）+ 搜索框（按 hash / 消息关键字过滤）。
- [ ] 状态展示：主窗口/仓库菜单显示 `git status` 摘要。
- [ ] fetch 按钮：手动刷新远程信息。

## 验收标准

- 分支、提交、状态信息正确显示，与 `git` 命令行结果一致。
- 切换分支后当前分支标记更新。
- 提交列表可滚动加载、可搜索。

## 技术注意事项

- 所有 git 调用统一走 subprocess，不引入 libgit2。
- 提交列表用 `git log --pretty=format:%H%x09%an%x09%ad%x09%s --date=iso` 之类固定格式解析，避免多语言环境解析失败（加 `-c core.quotepath=false` 与 `LC_ALL=C` 环境）。
- 远程分支标注 `origin/xxx`，当前分支高亮。

## 产出文件

- `src/git/gitclient.h/.cpp`
- `src/ui/branchdialog.h/.cpp`
- `src/ui/commitdialog.h/.cpp`
