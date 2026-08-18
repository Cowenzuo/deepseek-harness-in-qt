# Phase 3 — 进程管理

> 状态：✅ **已完成**（对应 commit `9a5a4ad`）
> 本阶段从早期「QProcess 子进程」设计**演进为「常驻分离服务」模型**，本节为实际实现的定稿描述。

## 目标

实现 dsh 常驻服务全生命周期：环境检测与引导、分离启动、端口就绪等待、日志 tail 监督、停止/重启/接管。

## 前置条件

- Phase 2 完成（依赖设置）。

## 任务清单（实际结果）

- [x] `src/process/environmentchecker.{h,cpp}`：五项环境检测（异步逐项、信号实时反馈）：
  1. deepseek-harness 仓库路径：`sourcePath/pnpm-workspace.yaml` 存在；
  2. dsh 依赖：`node_modules/`、`apps/web/dist/`、`packages/**/lib`（任一非空 lib）存在；
  3. git：`git --version` 可解析；
  4. Node.js：`node -v` 解析且版本 ≥ 22.19（major>22 或 22.x≥19）；
  5. pnpm：`pnpm -v` 可解析。
  - `autoDetect`：PATH 探测回填 settings 空字段。
- [x] `src/process/preflightchecker.{h,cpp}`：依赖与构建产物体检（node_modules / apps/web/dist / packages lib 三项），供 `startService` 使用。
- [x] `src/process/startdetached.cpp` + `startwrapped.h`：分离启动——Windows `CreateProcessW + DETACHED_PROCESS + CREATE_NEW_PROCESS_GROUP`，stdout/stderr 句柄重定向 `config/dsh-web.log`，stdin→NUL，SW_HIDE；`.cmd/.bat/.ps1` shim 经 `cmd.exe /c` 包装；非 Windows `/bin/sh -c`。
- [x] `src/process/dshprocessmanager.{h,cpp}`：
  - 状态机：`Idle / Starting / Running / Stopping / Crashed`。
  - `start()`：异步 `killByPort` 清理残留 → `beginLaunch()`（`node --import tsx/esm apps/cli/src/bin.ts web --host 127.0.0.1 --port <N>`，工作目录=源码路径）→ 写 `service-source.txt` → ReadyWaiter 就绪轮询。
  - `ensureRunning()`：异步探测端口 → 匹配 attach / 反查接管 / 杀旧启新 / 启动。
  - `inspectAsync()`：netstat 找 PID → PowerShell `Get-CimInstance Win32_Process` 取命令行 → 命令行含 `bin.ts` 判定 dsh → 候选根探测（配置路径 → 命令行绝对路径 → 常见位置）识别源码根。
  - `killByPort()`：netstat 定位占用端口 PID → `taskkill /pid <pid> /t /f` 杀进程树（异步，全部完成回调）。
  - 监督：1s 定时器增量 tail `dsh-web.log` → `logOutput`。
  - `stop()`：停轮询 → 杀端口进程树 → 删 service-source.txt → Idle；`restart()`：杀旧 → 直接 beginLaunch。
  - 信号：`stateChanged`、`logOutput`。
- [x] `src/process/readywaiter.{h,cpp}`：`QNetworkAccessManager` 轮询 `http://127.0.0.1:<端口>`，**正面判定** body 含 `__DSH_BOOT__` 即就绪；1s 间隔，60s 超时；`probeOnce` 单次探测（回调式，用于 ensureRunning）。
- [x] `src/process/proxydetector.{h,cpp}`：系统代理检测（Windows 注册表 / Unix 环境变量）→ git `-c http.proxy=` 参数 / pnpm 代理环境变量。
- [x] `src/ui/setuppage.{h,cpp}`：引导页——环境失败时补全路径（源码/node/pnpm/git + 浏览按钮 + 逐项 ✓/✗ 状态）、一键构建、克隆仓库（`git clone --progress` → `pnpm install`）。
- [x] `src/ui/errorpage.{h,cpp}`：错误页——原因 + 重试 / 一键构建 / 查看日志。
- [x] `src/ui/logview.{h,cpp}`：日志页——等宽深色终端，错误行红色高亮，`\r` 进度覆盖符转行。
- [x] 一键构建：`pnpm install` → `pnpm run build`（注入代理环境变量），CLI 输出进日志页，完成后重新校验/启动。
- [x] 集成链路：首帧启动页 → `SetupPage.autoCheck` → 全过则启动服务 → Running 后台加载 webview → 锚点就绪显示主页；失败切引导页/错误页。

## 验收标准（结果）

- 能启动 dsh 并在主页加载出 Web UI（锚点就绪后才显示，不暴露早期加载警告）。
- 外壳退出后 dsh 继续运行；再次启动外壳自动连接（attach）或按端口接管/重启。
- 停止、重启、启动失败（Crashed → 错误页）均正确。
- 日志页实时显示进程输出（tail 模式）；端口不通时 60s 超时报错。

## 与早期设计的差异

| 早期设计 | 实际实现 |
| --- | --- |
| `QProcess` 子进程，外壳退出即结束 | **分离常驻服务**（DETACHED_PROCESS），关窗不杀，重启外壳自动连接 |
| 启动命令 = launchCommand + 追加 `--host/--port` | 固定 `node --import tsx/esm apps/cli/src/bin.ts web ...`，无 launchCommand |
| 崩溃判定：finished 信号 + 退出码 | 无子进程句柄：就绪超时 / 启动失败 → Crashed；状态由端口探测 + 日志 tail 监督 |
| 体检失败 → 提示 + 一键构建 | 引导页（SetupPage）：补路径 / 一键构建 / 克隆仓库 |
| 就绪判定：端口可访问 | `__DSH_BOOT__` 正面判定（服务层）+ 输入区/会话区锚点（界面层） |
| — | 新增端口反查接管（inspectAsync）、代理注入（ProxyDetector） |

## 产出文件

- `src/process/environmentchecker.h/.cpp`
- `src/process/preflightchecker.h/.cpp`
- `src/process/dshprocessmanager.h/.cpp`
- `src/process/readywaiter.h/.cpp`
- `src/process/proxydetector.h/.cpp`
- `src/process/startwrapped.h`
- `src/process/startdetached.cpp`
- `src/ui/setuppage.h/.cpp`
- `src/ui/errorpage.h/.cpp`
- `src/ui/logview.h/.cpp`
