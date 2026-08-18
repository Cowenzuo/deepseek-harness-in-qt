# deepseek-harness 本地客户端外壳（dshinqt）需求文档

> 文档状态：已同步至当前实现（对应 git commit `9a5a4ad`，2026-08-18 快照）。
> 本文按**实际代码行为**描述需求，早期设计文档中的差异点已在相应章节注明。
>
> **命名空间约定**：本工程全部 C++ 类型定义于 `namespace dshinqt` 内，正文省略前缀。

## 1. 概述与定位

本工程为 [deepseek-harness](https://github.com/deepseek-ai/deepseek-harness)（`dsh`）套一层**本地桌面外壳**，把 dsh 从命令行变成**常驻后台服务**：用图形界面完成环境检测、仓库获取/更新与服务管理，内置 Web 界面（QtWebEngine 渲染），无需记忆任何命令。

外壳只做四件事：

- **环境引导**：检测 git / node / pnpm / dsh 仓库与依赖构建产物，缺失时引导补全路径、一键构建或克隆仓库
- **启动** dsh（常驻后台，关窗不杀）
- **更新** dsh（拉取 / 切换分支或提交 / 重编译 / 重启）
- **Git 查看**（分支、提交、同步状态、脏状态）

dsh 自身的业务配置（API key、模型等）一律交给它的 Web UI，外壳不干预。

## 2. 技术栈

- 界面：**Qt Widgets（C++）**，深色主题（Fusion + 自定义 palette）
- 内嵌浏览器：**QtWebEngine**（`QWebEngineView`），用于展示 dsh 的 Web UI
- 构建：CMake ≥ 3.21 + C++17；Windows 上 MSVC 2022 / VS 2026，Qt 6.11.1（msvc2022_64）
- 目标平台：**跨平台**（Windows 主验 / Linux / macOS 可编译）

## 3. 功能需求

### 3.1 启动与环境引导

- 外壳启动后**自动**走「环境检测 → 启动服务」流程，无需手动点击。
- 首屏为启动加载页（不确定进度条 + 状态文字），随后异步逐项检测环境（不阻塞 UI）：
  1. **deepseek-harness 仓库路径**：目录含 `pnpm-workspace.yaml`
  2. **dsh 依赖**：源码根 `node_modules/`、前端产物 `apps/web/dist/`、库产物 `packages/**/lib` 均存在
  3. **git**：`git --version` 可解析
  4. **Node.js**：`node -v` 可解析且版本 ≥ 22.19
  5. **pnpm**：`pnpm -v` 可解析
- 全部通过 → 自动启动服务；任一失败 → 切到**引导页**，逐项显示 ✗ 与原因，提供：
  - 路径补全（源码 / node / pnpm / git 四项，可浏览选择，留空 = 用 PATH）
  - **一键构建依赖**（源码有效但依赖/产物缺失时：`pnpm install` + `pnpm run build`）
  - **克隆仓库**（源码目录不存在或为空，且仓库地址有效时：`git clone` + `pnpm install`）
- 服务启动成功后**后台加载** Web UI，页面真正就绪（输入区/会话区挂载）后才显示，避免用户看到 dsh 早期加载的警告。
- 启动受阻（依赖缺失 / 端口超时 / 进程启动失败）→ **错误页**：原因说明 + 「重试 / 一键构建 / 查看日志」。

### 3.2 常驻服务管理

- dsh 以**分离后台进程**方式启动（Windows 用 `DETACHED_PROCESS`），外壳退出后 dsh 继续运行；再次启动外壳时自动**连接已运行的服务**。
- 服务监督不依赖子进程句柄：**异步端口探测** + **日志文件 tail**（增量读取 `config/dsh-web.log`），全程不阻塞主线程。
- 端口上已有服务时自动**反查**（netstat 定位 PID → 取命令行 → 判定是否为 dsh）：
  - 与记录源码路径一致 → 直接连接（attach）；
  - 是 dsh 但源码路径不一致 → 尝试识别源码根并**自动接管**（更新配置）；
  - 不是 dsh / 无法识别 → **杀旧启新**（按端口找 PID，`taskkill /t /f` 杀进程树）。
- 提供启动 / 停止 / 重启（设置弹窗「服务」页 + 状态栏）；停止同样按端口清理进程树。
- 服务日志实时进入日志页（等宽深色终端样式，错误行红色高亮）。

### 3.3 设置与持久化

- 原则：**能放进设置的都放进设置**；所有设置项持久化到本地配置文件，启动时读取、修改即保存。
- 设置项（`config/config.json`，位于**可执行文件旁**，绿色软件跟随 exe）：

| 字段 | 默认值 | 说明 |
| --- | --- | --- |
| 源码路径 `sourcePath` | 空 | 启动时探测不到 → 引导页配置；须含 `pnpm-workspace.yaml` |
| Web UI 端口 `webPort` | `3080` | 用于就绪轮询、页面加载与启动参数；取值 1–65535 |
| node 路径 `nodePath` | 空（从 PATH 查找） | 可显式指定 |
| pnpm 路径 `pnpmPath` | 空（从 PATH 查找） | 可显式指定 |
| git 路径 `gitPath` | 空（从 PATH 查找） | 可显式指定 |
| 仓库地址 `repoUrl` | `https://github.com/deepseek-ai/deepseek-harness.git` | 克隆与校验用 |

> 演进说明：早期设计有「启动命令 `launchCommand`」设置项（默认 `pnpm dsh web`）；
> 现实现已移除——启动命令固定为 `node --import tsx/esm apps/cli/src/bin.ts web`，
> 由外壳直接调用 node，绕过 pnpm/cmd 中间层。

- 校验：源码路径存在且含 `pnpm-workspace.yaml`；端口 1–65535。
- 配置文件为 JSON（Indented 格式），存于 `QCoreApplication::applicationDirPath()/config/config.json`。
  > 演进说明：早期设计用 `QStandardPaths::AppConfigLocation`，现改为 exe 旁 `config/` 目录。

### 3.4 更新

- **一键全自动**，流程：停止 dsh（若运行）→ `git fetch --all --prune` → 切换到选定分支或提交 →（分支目标）`git pull --ff-only` → `pnpm install` → `pnpm run build` → 重新启动 dsh。
- 目标选择（全部在设置弹窗「更新」页内完成，不弹子窗口）：
  - **分支**：分支树（本地/远程分组，当前分支加粗）选中 → 「切换到该分支」；或「更新当前分支」直接拉取更新。
  - **提交**：提交列表（每批 60 条）选中 → 「切换到该提交」/双击行；切换时自动创建 `dsh/yyyyMMdd-<hash7>` 分支，**避免 detached HEAD**，使切换后的版本可继续更新。
- 本地存在未提交修改（`git status --porcelain` 非空）时，**直接拒绝更新**并提示。
- 更新过程显示阶段状态（停止 / 拉取 / 切换 / 合并更新 / 装依赖 / 构建 / 启动），日志实时滚动；任一步失败即中止，错误日志红色高亮 + 弹窗提示并切到日志页。
- 更新成功后 dsh 重新运行，Web UI 自动重新加载。

### 3.5 Git 查看

- **分支树**：本地 + 远程分支分组展示，当前分支加粗高亮；点击分支 → 异步加载该分支提交列表。
- **提交列表**：当前分支/所选分支 log，展示 hash（7 位）、提交消息、作者、日期；点击分支或切换后自动刷新。
- **同步状态**：`rev-list --left-right --count HEAD...@{upstream}` 计算与上游的**领先/落后**提交数（落后 → 提示「有更新可拉取」；领先 → 提示；同步 → 绿色；无上游 → 提示），工作区脏时附「工作区有改动」标记。
- **Fetch 刷新**：手动按钮执行 `git fetch --all --prune`（自动注入系统代理），完成后刷新仓库快照。
- 仓库快照用 `QtConcurrent::run` **后台线程采集**，不阻塞 UI。

### 3.6 界面

- 主窗口（`QMainWindow`）：
  - **主页**：`QWebEngineView` 充满窗口，加载 `http://127.0.0.1:<端口>`（dsh Web UI）。
  - **启动页 / 引导页 / 错误页 / 日志页**：`QStackedWidget` 分页，与 webview **平级叠放**（raise/lower 切换），webview 常驻渲染避免切页白闪。
  - 状态栏：左侧「>」圆形按钮（打开设置）+ 右侧 dsh 运行状态文字。
  - 无菜单栏（早期设计的「文件/仓库/更新/设置/帮助」菜单已随架构演进移除，功能并入设置弹窗）。
- 设置弹窗：左侧竖向导航（常规 / 服务 / 更新 / 关于）+ 右侧页面；**更新页即 Git 查看页**，一页内完成查看与操作。

## 4. 非功能需求

- 跨平台：Windows / Linux / macOS（Windows 主验，其余平台至少可编译）。
- 不修改 dsh 源码，仅通过 git 与 pnpm/node 命令操作。
- **常驻服务**：dsh 独立于外壳运行，外壳退出不影响服务。
- 深色界面，无白闪（OpenGL 渲染后端统一 + 共享 OpenGL 上下文 + webview 常驻渲染）。
- 所有网络操作（git fetch/clone、pnpm 安装）自动注入**系统代理**。

## 5. 待办（Phase 6 未完成项）

以下早期规划项当前**尚未实现**，代码中无对应逻辑：

- 单实例：`QLockFile` 防重复启动（同一配置目录只允许一个外壳进程）。
- 启动时后台自动 `fetch` 并提示「可更新」（目前只能手动 Fetch）。
- 状态栏显示当前分支（目前仅显示运行状态）。
- 崩溃提示的「最后日志摘要」（目前错误页仅给出原因与日志入口）。

## 6. 设计决策记录

| 议题 | 决策 |
| --- | --- |
| 壳框架 | Qt Widgets + QtWebEngine（C++） |
| 服务模型 | **常驻分离进程**（关窗不杀，重启外壳自动连接） |
| 服务监督 | 异步端口探测 + 日志 tail，不依赖子进程句柄 |
| 启动命令 | 固定 `node --import tsx/esm apps/cli/src/bin.ts web`，无 launchCommand 设置项 |
| 环境缺失 | 引导页：补全路径 / 一键构建 / 克隆仓库 |
| 更新方式 | 一键全自动（连停带起），分支目标追加 `pull --ff-only` |
| 切换提交 | 自动建 `dsh/yyyyMMdd-<hash7>` 分支，避免 detached HEAD |
| 本地有未提交修改 | 直接拒绝更新 |
| 配置持久化 | JSON + exe 旁 `config/config.json`（绿色软件） |
| Web UI 端口 | 默认 3080，设置中可改 |
| Git 查看入口 | 并入设置弹窗「更新」页（分支树 + 提交列表 + 同步状态） |
| 主界面 | 无菜单栏：启动/引导/错误/日志四页 + webview 叠放 + 状态栏 |
| 页面就绪判定 | 服务层判 `__DSH_BOOT__`，界面层判正面锚点（输入区+会话区），超时自愈 reload |
| 代理 | git 注入 `-c http.proxy/-c https.proxy`；pnpm 注入代理环境变量 |
| 单实例 | **未实现**（待 Phase 6） |
| 自动 fetch | **未实现**（待 Phase 6） |
