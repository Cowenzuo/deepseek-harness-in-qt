# Phase 2 — 设置管理

> 状态：✅ **已完成**（对应 commit `9a5a4ad`）
> 设置项与存储位置已相对早期设计变化，本节为**实际实现**的定稿描述。

## 目标

实现配置结构、JSON 持久化与设置对话框：启动时读取、修改即写回，并对关键字段做校验。

## 前置条件

- Phase 1 完成。

## 任务清单（实际结果）

- [x] `src/settings/appsettings.{h,cpp}`：
  - 字段：`sourcePath`、`webPort`（默认 3080）、`nodePath`、`pnpmPath`、`gitPath`、`repoUrl`（默认 `https://github.com/deepseek-ai/deepseek-harness.git`）。
  - `load()`/`save()`：`QJsonDocument` 读写，文件 = **`<可执行目录>/config/config.json`**（绿色软件跟随 exe，非早期设计的 `QStandardPaths::AppConfigLocation`）。
  - 缺字段回退默认值；`save()` 自动 mkpath。
- [x] `src/settings/settingsdialog.{h,cpp}`：统一设置弹窗 = 左侧竖向导航（⚙常规 / ◉服务 / ⇄更新 / ℹ关于）+ 右侧 `QStackedWidget` 页面：
  - **常规**：仓库路径 / Web UI 端口（QSpinBox 1–65535）/ node / pnpm / git / 仓库地址 + 「保存配置」。
  - **服务**：状态灯 + PID + 源码路径 + 端口 + 服务地址 + 日志路径 + 启动/停止/重启按钮；打开时 `inspectAsync` 反查 PID/源码。
  - **更新**：Git 查看与更新操作页（见 Phase 4/5）。
  - **关于**：软件定位、特性列表、上游仓库。
- [x] 校验：源码路径须存在且含 `pnpm-workspace.yaml`（保存时校验，失败弹警告）；端口 QSpinBox 限定 1–65535。
- [x] 入口：状态栏左侧「>」按钮打开；保存即写回文件。
- [x] 启动时 `load()`；环境缺失 → 引导页（`SetupPage`）补全并校验（Phase 3 内容）。

## 验收标准（结果）

- 修改设置并重启后，各字段值被正确保留（`install/bin/config/config.json` 实测生效）。
- 非法路径被拒绝并提示；非法端口无法输入（spin 范围限定）。
- 默认值正确：3080、空 node/pnpm/git 路径、默认 repoUrl。
- 可执行环境项自动探测回填：`EnvironmentChecker::autoDetect`（PATH 查找 node/pnpm/git，只填空字段）。

## 与早期设计的差异

| 早期设计 | 实际实现 |
| --- | --- |
| `launchCommand`（默认 `pnpm dsh web`） | **已移除**；启动命令固定 node 直启（见 Phase 3） |
| `QStandardPaths::AppConfigLocation` | exe 旁 `config/config.json` |
| 简单 QDialog + QFormLayout | 四页导航式弹窗（常规/服务/更新/关于） |
| — | 新增 `gitPath`、`repoUrl` 字段 |

## 产出文件

- `src/settings/appsettings.h`、`src/settings/appsettings.cpp`
- `src/settings/settingsdialog.h`、`src/settings/settingsdialog.cpp`
