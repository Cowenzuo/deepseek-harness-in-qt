# deepseek-harness-in-qt（dshinqt）

> ⚠️ **状态：已封存（Archived）**
>
> Qt + QtWebEngine 内嵌路线终止。封存原因：Web UI 承载需要壳侧大量特判
> （服务认证适配、下载/剪贴板权限承接、页面就绪判定与自愈等），Qt 内嵌
> 不是承载 dsh Web UI 的最优方案。
>
> 后续方向：**v2 Web 客户端壳**（与 VS Code 同源的底层 web 技术）。
> 需求规格见 [`docs/需求规格-Web客户端壳.md`](docs/需求规格-Web客户端壳.md)。
>
> 本仓库保持只读：不再新增功能，历史实现与工具链保留备查（会话修复脚本
> `tools/session-repair.mts` 为独立 node 脚本，与 Qt 无关，可继续使用）。

## 历史功能（v1 Qt 实现，供 v2 需求对照）

- **环境引导**：node / pnpm / git 自动探测，源码路径校验，引导页补全
- **服务编排**：dsh 常驻后台启动 / attach / 端口接管 / 停止重启 / 状态监督 / 超时兜底
- **Web UI 承载**：内嵌页面、token 认证自动适配、就绪判定、错误页与重试
- **工具链**：一键构建（clone/install/build）、启动前构建产物自检、更新流水线
- **会话修复**：GUID 定位 + 诊断 + 备份修复（seq 重复/重叠/缺口形态自动识别与验证）
- **体验**：下载目录可配、剪贴板可用、深色主题、日志页、状态栏入口

## 技术栈（仅历史记录）

C++17 · Qt 6 Widgets + WebEngineWidgets · CMake · 单测 Qt Test（32 用例）

## 关键文档

| 文档 | 说明 |
| --- | --- |
| `docs/需求规格-Web客户端壳.md` | **v2 需求基线**（FR-1~39 + 非功能 + 验收要点） |
| `docs/requirment.md` / `docs/spec.md` | v1 需求/规格（历史快照） |
| `docs/回归报告-rc3.md` | v1 最后一次定版回归报告 |
