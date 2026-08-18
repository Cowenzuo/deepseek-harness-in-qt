# Phase 5 — 更新管理

## 目标

实现一键更新流水线：前置检查 → 停 dsh → fetch → 切换 → install → build → 重启，全程进度与日志展示。

## 前置条件

- Phase 3（进程管理）、Phase 4（Git 查看）完成。

## 任务清单

- [ ] 编写 `src/update/updatemanager.{h,cpp}`：状态机
  `Idle → Stopping → Fetch → Checkout → Install → Build → Starting → Done/Failed`。
- [ ] 前置检查：
  - `isDirty()` 非空 → 拒绝更新并提示。
  - dsh 运行中 → 自动停止（连停带起）。
- [ ] 分阶段执行（每步失败即中止，信号 `stageChanged`、`finished`）：
  1. `git fetch --all --prune`
  2. checkout 分支或指定提交（复用 Phase 4 的 BranchDialog / CommitDialog 选目标）
  3. `pnpm install`
  4. `pnpm run build`
  5. 重新启动 dsh
- [ ] 进度展示：整体进度条 + 分阶段标签（拉取/切换/装依赖/构建/启动），日志实时滚动。
- [ ] 失败处理：中止后续步骤，错误日志高亮。
- [ ] 更新成功后：自动刷新主页 Web UI（`QWebEngineView::reload()`）。

## 验收标准

- 一键更新跑通全流程（可对测试分支验证）。
- 任一步失败（如断网、install 报错）时正确中止并展示错误。
- 更新成功后 dsh 重新运行且页面刷新。

## 技术注意事项

- 长耗时步骤（install/build）用 QProcess 的 `readyRead` 增量读输出，避免阻塞 UI；步骤间用信号槽串联，不要用 `waitForFinished` 阻塞主线程。
- 步骤状态与整体进度条解耦：进度条用「不确定」模式 + 阶段标签文字。
- 构建命令 `pnpm run build` 工作目录 = 源码路径，环境变量需能找到 pnpm（设置里 pnpmPath 非空时拼全路径）。

## 产出文件

- `src/update/updatemanager.h`
- `src/update/updatemanager.cpp`
