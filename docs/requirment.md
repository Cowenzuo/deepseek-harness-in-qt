# deepseek-harness 本地客户端外壳（dshinqt）需求文档

## 1. 概述与定位

本工程为 [deepseek-harness](https://github.com/deepseek-ai/deepseek-harness)（`dsh`）套一层**本地桌面外壳**，方便快速启动、更新与查看版本信息。

外壳只做三件事：

- **启动** dsh
- **更新** dsh（拉取 / 切换分支或提交 / 重编译）
- **Git 查看**（分支、提交、状态）

dsh 自身的业务配置（API key、模型等）一律交给它的 Web UI，外壳不干预。

## 2. 技术栈

- 界面：**Qt Widgets（C++）**
- 内嵌浏览器：**QtWebEngine**（`QWebEngineView`），用于展示 dsh 的 Web UI
- 目标平台：**跨平台**（Windows / Linux / macOS）

## 3. 功能需求

### 3.1 设置与持久化

- 原则：**能放进设置的都放进设置**；所有设置项持久化到本地配置文件，启动时读取、修改即保存。
- 设置项：
  1. deepseek-harness 源码路径（默认 `D:\framework\deepseek-harness`，探测不到时由用户在设置中手动配置）
  2. 启动命令（默认 `pnpm dsh web`，可修改）
  3. Web UI 端口（默认 `3080`）
  4. node / pnpm 可执行文件路径（默认从 PATH 查找，可显式指定）
- 配置文件：**JSON** 格式，存放于跨平台标准配置目录 `QStandardPaths::AppConfigLocation`。

### 3.2 启动

- 外壳**全接管** dsh 进程生命周期：启动、停止、重启、崩溃提示。
- 启动前体检：检查 node/pnpm 版本、依赖是否安装、构建产物是否存在；缺失时提示用户先处理。
- 启动后轮询 Web UI 端口，服务就绪后再加载网页。

### 3.3 更新

- **一键全自动**，流程：停止 dsh → 拉取 / 切换到选定分支或提交 → `pnpm install` → `pnpm run build` → 重新拉起 dsh。
- 支持选定分支或指定提交版本。
- 本地存在未提交修改时，**直接拒绝更新**并提示。
- 更新过程显示进度条；失败时展示错误日志。

### 3.4 Git 查看

- 分支列表：本地 + 远程分支，当前分支高亮。
- 提交列表：显示 hash、作者、时间、提交消息，字段尽可能全面，支持分页。
- 工作区状态（`status`）。
- 手动 `fetch` 刷新按钮。

### 3.5 界面

- 顶部菜单栏（含「设置」等入口）。
- 主区域由 `QWebEngineView` 充满，展示 dsh 的 Web UI。

## 4. 非功能需求

- 跨平台：Windows / Linux / macOS。
- 外壳仅套壳，不修改 deepseek-harness 源码（仅通过 git 与 pnpm 命令操作）。

## 5. 设计决策记录

| 议题 | 决策 |
| --- | --- |
| 壳框架 | Qt Widgets + QtWebEngine（C++） |
| 更新方式 | 一键全自动（连停带起） |
| 本地有未提交修改 | 直接拒绝更新 |
| 配置持久化 | JSON + `QStandardPaths::AppConfigLocation` |
| Web UI 端口 | 默认 3080，设置中可改 |
| 主界面 | 菜单栏 + webview 充满整个区域 |
