# Phase 6 — 收尾打磨

> 状态：⏳ **部分完成**
> 已完成：跨平台可编译性（非 Windows 分支已就位）、错误页与日志入口、深色无白闪打磨。
> 未完成项见「剩余任务」，是当前代码与 docs 规划之间**已知缺口**。

## 目标

补齐非功能需求：单实例、启动自动 fetch 提示、状态栏信息、崩溃提示完善、跨平台验证。

## 任务清单（实际结果）

- [x] 深色无白闪打磨：`AA_ShareOpenGLContexts` + OpenGL 渲染后端 + Fusion 深色 palette + webview 不透明深色背景 + 常驻渲染叠放（Phase 1 起持续完善）。
- [x] 崩溃/失败提示：启动失败 → 错误页（原因 + 重试/一键构建/查看日志）；更新失败 → 弹窗 + 日志页。
- [x] 跨平台可编译：`killByPort`/`inspectAsync` 非 Windows 直接回调空结果；`startdetached.cpp` 提供 `/bin/sh -c` 分支；`ProxyDetector` Unix 走环境变量。
- [x] 状态栏：显示 dsh 运行状态（未启动/启动中/运行中/停止中/启动失败）与更新阶段文字。
- [x] 清理死代码：删除无引用的 `src/ui/branchdialog.*`、`src/ui/commitdialog.*`（功能已并入设置弹窗「更新」页）。
- [ ] **单实例**：`QLockFile`（配置目录锁文件）防重复启动——**未实现**（规划：`main.cpp` 入口加锁，重复启动退出）。
- [ ] **启动时后台自动 fetch**，发现远程有新提交提示「可更新」——**未实现**（目前仅设置弹窗内手动 Fetch）。
- [ ] **状态栏显示当前分支**——**未实现**（目前状态栏仅运行状态；分支信息在设置弹窗更新页可见）。
- [ ] **崩溃提示日志摘要**：进程异常时给出最后日志摘要——**未实现**（错误页仅给原因与日志入口）。
- [ ] 跨平台运行验证：Windows 已完整验证；Linux/macOS 仅代码路径就位，未实机编译验证。

## 验收标准（当前缺口）

- 重复启动时只保留一个实例 —— ✗ 未达成
- 启动后自动 fetch 并提示是否有更新 —— ✗ 未达成
- 状态栏信息随运行状态/分支实时更新 —— ◐ 仅运行状态达成
- 崩溃提示含日志摘要 —— ✗ 未达成（有错误页与日志入口）
- Windows 完整可用；其余平台可编译 —— ◐ Windows 达成，其余平台未验证

## 剩余任务（实施建议）

1. **单实例**：`QLockFile` 锁文件放 `config/` 目录（或 `QStandardPaths`），`tryLock` 失败即退出；可配合单实例激活已有窗口。
2. **启动自动 fetch**：`MainWindow` 启动流程完成后异步 `GitClient::fetch`，成功后 `aheadBehind` 判定落后 → 状态栏/提示「可更新」。
3. **状态栏分支**：复用 `GitClient::currentBranch`（异步采集避免阻塞），随更新/切换刷新。
4. **崩溃日志摘要**：`DshProcessManager` Crashed 时携带 `dsh-web.log` 末尾若干行，错误页展示。
5. 条件允许时在 Linux/macOS 实机验证编译与运行。

## 产出文件（已完成部分涉及）

- `src/main.cpp`（深色主题、OpenGL、debug.log）
- `src/mainwindow.h/.cpp`（状态栏、错误页集成、叠放切换）
- `src/process/dshprocessmanager.h/.cpp`（常驻模型、Crashed 路径）
- `src/process/startdetached.cpp`（跨平台分离启动）
- `src/process/proxydetector.cpp`（跨平台代理检测）
