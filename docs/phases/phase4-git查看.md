# Phase 4 — Git 查看

> 状态：✅ **已完成**（对应 commit `9a5a4ad`）
> 查看界面从「独立对话框」**演进为「设置弹窗更新页」**，本节为实际实现的定稿描述。

## 目标

实现 git 命令封装与分支/提交/状态查看界面。

## 前置条件

- Phase 1 完成；与 Phase 2/3 并行集成。

## 任务清单（实际结果）

- [x] `src/git/gitclient.{h,cpp}`：QProcess 同步调系统 `git`（20s 超时，自动注入代理参数 `-c http.proxy=... -c https.proxy=...`）：
  - `branches()`：本地（`for-each-ref refs/heads`）+ 远程（`refs/remotes`），标注当前分支；
  - `commits(rev, limit, offset)`：`git log --pretty=format:%H%x09%an%x09%ad%x09%s --date=short [rev] --skip=<offset> -n <limit>`，按 `\t` 解析四段；
  - `searchCommits(keyword)`：全十六进制（≥4 位）按 hash 精确查，否则 `--all --grep`；
  - `statusSummary()` / `isDirty()`（`status --porcelain` 非空）/ `currentBranch()`；
  - `aheadBehind(ahead, behind)`：`rev-list --left-right --count HEAD...@{upstream}`；
  - `fetch()`：`git fetch --all --prune`（120s 超时）；
  - `checkoutBranch(name)`：剥 `origin/` 前缀后 checkout；
  - `checkoutCommit(hash)`：建 `dsh/yyyyMMdd-<hash7>` 分支再检出（避免 detached HEAD；同名分支已存在则直接检出）。
- [x] 查看界面并入 `SettingsDialog`「更新」页（`buildRepoUpdateTab`）：
  - 顶栏：当前分支 + 同步状态（落后 N 个提交「有更新可拉取」/ 领先 / 已同步 / 无上游 / 工作区有改动）+ 「Fetch 刷新」「更新当前分支」；
  - 左侧**分支树**：本地/远程分组，当前分支加粗；点击分支 → 异步加载该分支提交；
  - 右侧**提交列表**：每批 60 条，双击行或「切换到该提交」；
  - 仓库快照（分支/提交/领先落后/脏）由 `QtConcurrent::run` 后台线程采集，`QFutureWatcher` 回主线程刷新（`collectSnapshot`）。
- [x] fetch 按钮：手动刷新远程信息（成功后重新采快照；失败红字提示）。
- [x] 遗留清理：早期实现的 `src/ui/branchdialog.*`、`src/ui/commitdialog.*` 已无任何引用，**已删除**。

## 验收标准（结果）

- 分支、提交、同步状态信息正确显示，与 `git` 命令行结果一致。
- 切换分支后当前分支标记更新（更新流程见 Phase 5）。
- 提交列表按分支加载、双击可切换。

## 技术注意事项（落地要点）

- 所有 git 调用统一走 subprocess，不引入 libgit2；自动注入系统代理。
- `git log` 固定格式 + `\t` 分隔解析，避免多语言环境解析失败。
- 仓库信息采集放后台线程，不阻塞 UI。

## 产出文件

- `src/git/gitclient.h`、`src/git/gitclient.cpp`
- `src/settings/settingsdialog.cpp`（更新页：分支树 + 提交列表 + 同步状态）
- 已删除：`src/ui/branchdialog.h/.cpp`、`src/ui/commitdialog.h/.cpp`（死代码）
