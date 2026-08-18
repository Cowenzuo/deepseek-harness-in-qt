# Phase 5 — 更新管理

> 状态：✅ **已完成**（对应 commit `9a5a4ad`）
> 流程比早期设计**新增 Pull 阶段**，切换提交行为也有变化，本节为实际实现的定稿描述。

## 目标

实现一键更新流水线：前置检查 → 停 dsh → fetch → 切换 → （分支 pull）→ install → build → 重启，全程日志展示。

## 前置条件

- Phase 3（进程管理）、Phase 4（Git 查看）完成。

## 任务清单（实际结果）

- [x] `src/update/updatemanager.{h,cpp}`：状态机 `Idle → Stopping → Fetch → Checkout → Pull → Install → Build → Starting → Done / Failed`。
- [x] 前置检查：
  - `isDirty()` 非空 → 拒绝更新并提示（`fail()`）；
  - dsh 运行中 → 自动 `stop()`（按端口杀进程树）。
- [x] 分阶段执行（每步失败即中止；信号 `stageChanged`、`logOutput`、`finished`）：
  1. `git fetch --all --prune`（注入代理参数，QProcess 异步 + 增量输出）；
  2. 切换目标：分支 → `checkoutBranch`（剥 `origin/` 前缀）；提交 → `checkoutCommit`（自动建 `dsh/yyyyMMdd-<hash7>` 分支，**避免 detached HEAD**）；
  3. **分支目标追加** `git pull --ff-only`（`Pull` 阶段，提交目标跳过）；
  4. `pnpm install`；
  5. `pnpm run build`；
  6. `DshProcessManager::start()`，等 `Running` → `Done`；`Crashed` → `Failed`。
- [x] 进度展示：状态栏 `更新：<阶段文字>`（停止 dsh/拉取/切换/合并更新/装依赖/构建/启动/完成/失败），日志实时滚动到日志页。
- [x] 失败处理：中止后续步骤，错误日志红色高亮 + `QMessageBox::warning` + 切日志页。
- [x] 更新成功后：dsh 重新 Running → MainWindow 自动重新 load 主页 Web UI（`onDshStateChanged` 驱动）。
- [x] 入口集成（设置弹窗「更新」页）：「更新当前分支」、「切换到该分支」（分支树选中）、「切换到该提交」/双击提交行——全部在对话框内发起，后台执行，对话框保留。

## 验收标准（结果）

- 一键更新跑通全流程（切换分支 / 更新当前分支 / 切换提交三条路径）。
- 任一步失败（断网、install 报错等）时正确中止并展示错误。
- 更新成功后 dsh 重新运行且页面刷新。

## 技术注意事项（落地要点）

- 长耗时步骤（fetch/install/build）全部 QProcess 异步（`readyReadStandardOutput` 增量读输出），步骤间信号槽串联，不用 `waitForFinished` 阻塞主线程。
- pnpm 为 `.cmd` shim 时经 `cmd.exe /c` 包装执行（`startWrapped`）；构建环境变量注入系统代理（`HTTPS_PROXY/HTTP_PROXY`）。
- `fail()`/`done()` 后状态回到 `Idle`，允许再次发起。

## 与早期设计的差异

| 早期设计 | 实际实现 |
| --- | --- |
| fetch → checkout → install → build → 启动 | 分支目标在 checkout 后**追加 `git pull --ff-only`**（`Pull` 阶段） |
| checkout 指定提交（detached HEAD） | 自动建 `dsh/yyyyMMdd-<hash7>` 分支，避免 detached HEAD |
| BranchDialog/CommitDialog 选目标 | 设置弹窗「更新」页内直接选择 |

## 产出文件

- `src/update/updatemanager.h`、`src/update/updatemanager.cpp`
- `src/settings/settingsdialog.cpp`（入口集成）
- `src/mainwindow.cpp`（状态栏阶段文字、失败弹窗、成功刷新）
