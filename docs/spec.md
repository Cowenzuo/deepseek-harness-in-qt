# deepseek-harness 本地客户端外壳（dshinqt）规格说明（定版）

> 文档状态：已同步至当前实现（对应 git commit `9a5a4ad` 及后续 refactor）。
> 本文为**实现级规格**，与代码行为一一对应。
>
> **命名空间约定**：本工程全部 C++ 类型定义于 `namespace dshinqt` 内，正文省略前缀。

## 1. 概述

本工程为 [deepseek-harness](https://github.com/deepseek-ai/deepseek-harness)（`dsh`）套一层本地桌面外壳，提供环境引导、启动（常驻后台服务）、更新、Git 查看四类能力。dsh 自身业务配置（API key、模型等）由其 Web UI 承担，外壳不干预。

## 2. 设置管理

### 2.1 设置项

| 字段 | JSON 键 | 默认值 | 说明 |
| --- | --- | --- | --- |
| 源码路径 | `sourcePath` | 空 | 须含 `pnpm-workspace.yaml`；缺失时引导页补全 / 克隆 |
| Web UI 端口 | `webPort` | `3080` | 就绪轮询、页面加载与启动参数共用；1–65535 |
| node 路径 | `nodePath` | 空（PATH 查找） | 可显式指定 |
| pnpm 路径 | `pnpmPath` | 空（PATH 查找） | 可显式指定 |
| git 路径 | `gitPath` | 空（PATH 查找） | 可显式指定 |
| 仓库地址 | `repoUrl` | `https://github.com/deepseek-ai/deepseek-harness.git` | 克隆与克隆按钮显示判定用 |

### 2.2 持久化

- 文件：`<可执行目录>/config/config.json`（`QCoreApplication::applicationDirPath()/config`），JSON Indented 格式。
- 启动时 `load()`（缺字段回退默认值）；修改即 `save()` 写回。
- 可执行路径的环境项（node/pnpm/git）由 `EnvironmentChecker::autoDetect` 在启动时**回填空字段**（PATH 探测，不覆盖已有值）。

### 2.3 入口与校验

- 入口：状态栏左侧「>」按钮 → 设置弹窗「常规」页。
- 校验：源码路径须存在且含 `pnpm-workspace.yaml`；端口用 QSpinBox 限定 1–65535；保存时校验失败弹警告。

## 3. 启动与常驻服务管理

### 3.1 环境检测（EnvironmentChecker）

启动首帧显示启动页，随后异步逐项检测（信号 `checkStarted/itemChecked/checkCompleted` 实时反馈），顺序：

| # | 检测项 | 判定 |
| --- | --- | --- |
| 1 | deepseek-harness 仓库路径 | `sourcePath/pnpm-workspace.yaml` 存在 |
| 2 | dsh 依赖 | `node_modules/`、`apps/web/dist/` 存在，且 `packages/**/lib` 存在非空目录 |
| 3 | git | `git --version` 可解析 |
| 4 | Node.js | `node -v` 解析出版本 ≥ 22.19（major>22 或 major==22 且 minor≥19） |
| 5 | pnpm | `pnpm -v` 可解析 |

全部通过 → 写回 settings 并保存 → 启动服务；任一失败 → 切引导页（`SetupPage`）。

### 3.2 引导页（SetupPage）

- 字段：仓库路径 / Node.js / pnpm / git（可浏览选择），每个字段旁实时状态（✓ 绿 / ✗ 红 / ⏳ 校验中）。
- 动作：
  - **校验**：重新触发逐项检测；
  - **一键构建依赖**：`pnpm install` → `pnpm run build`（源码路径有效但依赖/产物缺失时显示），完成后重新校验；
  - **克隆仓库**：`git clone --progress <repoUrl> <sourcePath>` → `pnpm install`（源码目录不存在或为空、且 repoUrl 有效时显示），完成后重新校验。
- 校验通过 → 保存配置 → 启动服务。

### 3.3 服务状态机（DshProcessManager）

`Idle → Starting → Running → Stopping`，异常路径到 `Crashed`。

| 状态 | 含义 |
| --- | --- |
| `Idle` | 未运行 |
| `Starting` | 分离启动中，端口就绪轮询中 |
| `Running` | 端口就绪（页面含 `__DSH_BOOT__`） |
| `Stopping` | 停止中（按端口杀进程树） |
| `Crashed` | 启动失败 / 就绪超时 |

### 3.4 启动流程

```
ensureRunning()                     # 异步端口探测（probeOnce 单次 GET）
 ├─ 端口 up 且 service-source.txt 记录匹配 → attach()（直接 Running）
 ├─ 端口 up 但不匹配 → inspectAsync() 反查：
 │     netstat -ano 定位 PID → PowerShell Get-CimInstance 取命令行/可执行路径
 │     → 命令行含 bin.ts 判定是 dsh → 候选根探测（配置路径 → 命令行中绝对 bin.ts 路径 → 常见位置）
 │     → 命中 → 自动接管 sourcePath 并保存 + attach()
 │     → 未命中 → restart()（杀旧启新）
 └─ 端口不在 → start()
      └─ killByPort() 异步清理残留（netstat → taskkill /t /f）
          └─ beginLaunch()：
               node --import tsx/esm apps/cli/src/bin.ts web --host 127.0.0.1 --port <port>
               工作目录 = sourcePath；startDetachedWrapped() 分离启动
               stdout/stderr 重定向到 config/dsh-web.log（截断）
               写 service-source.txt（本次源码路径）
               ReadyWaiter.wait(127.0.0.1, port, 60000) 轮询就绪
```

- 就绪判定（ReadyWaiter）：HTTP GET `http://127.0.0.1:<port>` 响应体含 `__DSH_BOOT__` 即就绪（正面信号，不枚举 warning）；1s 间隔轮询，60s 超时 → `Crashed`。
- 监督：1s 定时器增量 tail `dsh-web.log`（记录文件偏移），新行经 `logOutput` 进日志页。
- 启动命令**固定**为 node 直启 dsh（无 launchCommand 设置项，端口由外壳追加）。

### 3.5 停止 / 重启 / 退出

- 停止：停轮询 → 按端口 netstat 找 PID → `taskkill /pid <pid> /t /f`（进程树）→ 删 `service-source.txt` → `Idle`。
- 重启：`killByPort` 完成后直接 `beginLaunch()`（不经过 Idle 中间态）。
- 外壳退出（`closeEvent`）：**不杀 dsh**，仅释放 webview（常驻服务）。

### 3.6 主页加载与就绪（HomePage）

- `Running` 后由 MainWindow 后台 `load()` Web UI；webview 与 stackwidget 平级叠放、常驻渲染（`lower` 被盖住），切页用 raise/lower。
- 就绪判定：`loadFinished` 后每 500ms 执行 `runJavaScript` 检查正面锚点——`[data-composer-card]`（输入区）与 `[data-conversation-scroll]`（会话区）**均挂载**即 `pageReady`（界面可用，MainWindow 才 raise 显示）；
  - 单周期 10s 未命中 → 自愈 `reload()` 一次；
  - 总超时 60s → `pageFailed` → 错误页。
- 调试：每次 `loadFinished` 后保存完整页面到 `config/webview-N.html`。

## 4. 更新管理

### 4.1 状态机（UpdateManager）

`Idle → Stopping → Fetch → Checkout → Pull(仅分支) → Install → Build → Starting → Done / Failed`

| 阶段 | 动作 |
| --- | --- |
| 前置检查 | `isDirty()` 非空 → 拒绝并提示；运行中 → 自动 `stop()` |
| `Fetch` | `git fetch --all --prune`（注入代理参数） |
| `Checkout` | 分支 → `checkoutBranch`（剥 `origin/` 前缀）；提交 → `checkoutCommit`（建 `dsh/yyyyMMdd-<hash7>` 分支，避免 detached HEAD；同名分支已存在则直接检出） |
| `Pull` | 仅分支目标：`git pull --ff-only` |
| `Install` | `pnpm install` |
| `Build` | `pnpm run build` |
| `Starting` | `DshProcessManager::start()`；等 `Running` → `Done`；`Crashed` → `Failed` |

- 长耗时步骤全部走 QProcess 异步（`readyReadStandardOutput` 增量输出到日志页），不用 `waitForFinished` 阻塞主线程。
- pnpm 为 shim（.cmd/.bat）时经 `cmd.exe /c` 包装执行（`startWrapped`）。
- 任一阶段失败 → 中止后续，`Failed` + 错误日志红色高亮 + 弹窗 + 切日志页。
- 更新成功后：dsh 重新 `Running` 时主页自动重新 load（由 `onDshStateChanged` 驱动）。

### 4.2 入口（设置弹窗「更新」页）

- 顶栏：当前分支 + 同步状态（领先/落后/已同步/无上游/工作区有改动）+ 「Fetch 刷新」「更新当前分支」。
- 左侧分支树：本地/远程分组，当前分支加粗；选中 → 加载该分支提交；双击或「切换到该分支」→ 更新流程。
- 右侧提交列表：每批 60 条；双击行或「切换到该提交」→ 更新流程。
- 仓库快照（分支/提交/领先落后/脏）由 `QtConcurrent::run` 后台线程采集，`QFutureWatcher` 回主线程刷新。

## 5. 主窗口

- 中央：`QStackedWidget`（启动页 / 引导页 / 错误页 / 日志页）+ `HomePage`（webview）平级叠放。
- 状态栏：左侧「>」设置按钮（16×16 圆形，点击开设置弹窗）+ 右侧状态 label（`dsh: 未启动/启动中/运行中/停止中/启动失败`；更新时显示 `更新：<阶段>`）。
- 无菜单栏。
- 设置弹窗（模态）：左侧竖向导航（⚙常规 / ◉服务 / ⇄更新 / ℹ关于）+ 右侧页面；服务页展示状态灯、PID、源码路径、端口、服务地址、日志路径与启动/停止/重启按钮（打开时异步反查 PID/源码）。

## 6. 非功能性需求

- 跨平台：Windows / Linux / macOS；Windows 主验（netstat/taskkill/PowerShell 反查为 Windows 专有路径，非 Windows 平台直接回调空结果并走启动流程）。
- 不修改 dsh 源码，仅通过 git、node、pnpm 命令操作。
- 单实例：**未实现**（规划中）。
- 深色无白闪：`AA_ShareOpenGLContexts` + `QQuickWindow::setGraphicsApi(OpenGL)`（QApplication 创建前）+ Fusion 深色 palette + webview 不透明深色背景 + 常驻渲染。
- 调试日志：qDebug 全部落 `<exe>/config/debug.log`（GUI 应用无控制台）。

## 7. 技术要点

- 进程：分离启动 `CreateProcessW + DETACHED_PROCESS + CREATE_NEW_PROCESS_GROUP`（Windows）/ `/bin/sh -c`（其他）；stdout/stderr 重定向到日志文件。
- 就绪轮询：`QNetworkAccessManager` 周期 GET；服务层判 `__DSH_BOOT__`，界面层判正面锚点。
- 配置：`QJsonDocument` + exe 旁 `config/config.json`。
- Git：subprocess 调系统 `git`，注入代理 `-c http.proxy=<proxy> -c https.proxy=<proxy>`。
- 代理：Windows 读注册表 `HKCU\...\Internet Settings`（ProxyEnable + ProxyServer，支持 `http=...;https=...` 多协议段；仅含 socks 等协议时跳过注入）；Unix 读 `HTTPS_PROXY` 等环境变量；pnpm 步骤额外注入 `HTTPS_PROXY/HTTP_PROXY` 环境变量。
- shim 包装：`.cmd/.bat/.ps1/无扩展名` 程序统一 `cmd.exe /c` 执行；`node.exe`/`git.exe` 等直接启动。

## 8. 设计决策

| 议题 | 决策 |
| --- | --- |
| 服务模型 | 常驻分离进程，关窗不杀；重启外壳自动连接/接管 |
| 服务监督 | 异步端口探测 + 日志 tail，不依赖子进程句柄 |
| 启动命令 | 固定 `node --import tsx/esm apps/cli/src/bin.ts web --host 127.0.0.1 --port N` |
| 端口联动 | 端口字段唯一来源，启动参数由外壳追加 |
| 环境缺失 | 引导页：补全路径 / 一键构建 / 克隆仓库 |
| 就绪判定 | 正面信号：`__DSH_BOOT__`（服务层）+ 输入区/会话区锚点（界面层），10s 自愈 reload，60s 超时 |
| 切换提交 | 自动建 `dsh/yyyyMMdd-<hash7>` 分支，避免 detached HEAD |
| 本地有未提交修改 | 拒绝更新 |
| 分支更新 | fetch 后 `pull --ff-only` |
| Git 查看 | 并入设置弹窗「更新」页，后台线程采快照 |
| 配置持久化 | JSON + exe 旁 `config/config.json` |
| 外部浏览器入口 | 不需要 |
| 自动启动 | 需要：外壳启动自动检测并启动/连接服务（已配置时） |
| 单实例 | 未实现（待 Phase 6） |
| 启动自动 fetch | 未实现（待 Phase 6，目前手动 Fetch） |
| 状态栏分支 | 未实现（待 Phase 6，目前仅运行状态） |
