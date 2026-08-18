# Phase 2 — 设置管理

## 目标

实现配置结构、JSON 持久化与设置对话框：启动时读取、修改即写回，并对关键字段做校验。

## 前置条件

- Phase 1 完成。

## 任务清单

- [ ] 编写 `src/settings/appsettings.{h,cpp}`：
  - 字段：`sourcePath`、`launchCommand`（默认 `pnpm dsh web`）、`webPort`（默认 3080）、`nodePath`、`pnpmPath`。
  - `load()` / `save()`：`QJsonDocument` 读写，路径用 `QStandardPaths::AppConfigLocation`。
  - 缺失字段回退默认值。
- [ ] 编写 `src/settings/settingsdialog.{h,cpp}`：`QDialog` + `QFormLayout`，编辑上述 5 项。
- [ ] 校验：源码路径存在且含 `pnpm-workspace.yaml`；端口取 1–65535。
- [ ] 菜单「设置」打开对话框，确定后保存即写回。
- [ ] 启动时 `load()`；源码路径探测不到时进入「未配置」状态并提示用户去设置。

## 验收标准

- 修改设置并重启后，各字段值被正确保留。
- 非法路径 / 非法端口被拒绝并提示。
- 默认值正确：`pnpm dsh web`、3080、空 node/pnpm 路径。

## 技术注意事项

- 配置文件路径：Windows 下 `QStandardPaths::AppConfigLocation` 落在 `%APPDATA%` 下，Linux/macOS 各有标准位置，无需手写平台分支。
- 设置对话框只做「读写与校验」，不触发启动/更新动作。

## 产出文件

- `src/settings/appsettings.h`
- `src/settings/appsettings.cpp`
- `src/settings/settingsdialog.h`
- `src/settings/settingsdialog.cpp`
