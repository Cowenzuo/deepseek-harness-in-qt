# Phase 3 — 进程管理

## 目标

实现 dsh 进程全生命周期管理：启动前体检、QProcess 启动、端口就绪等待、日志捕获、停止/重启/崩溃检测。

## 前置条件

- Phase 2 完成（依赖设置）。

## 任务清单

- [ ] 编写 `src/process/preflightchecker.{h,cpp}`：逐项检查
  - node 存在且版本 ≥ 22.19（解析 `node -v`）
  - pnpm 存在（解析 `pnpm -v`）
  - 依赖已安装：源码根 `node_modules/` 存在
  - 构建产物：`apps/web/dist/` 与 `packages/**/lib` 存在
  - 返回检查项结果列表（项名 + 通过/失败 + 提示语）。
- [ ] 编写 `src/process/dshprocessmanager.{h,cpp}`：
  - 状态机：`未启动/启动中/运行中/停止中/已退出(异常)`。
  - 启动：工作目录=源码路径，命令 = 启动命令主体 + `--host 127.0.0.1 --port <端口>`；剥离命令主体中手写的 `--host`/`--port`。
  - 信号：`stateChanged`、`logOutput`、`crashed`。
  - 停止：终止进程、等待退出、超时强杀。
  - 重启：stop 后 start。
- [ ] 编写 `src/process/readywaiter.{h,cpp}`：`QNetworkAccessManager` 轮询 `http://127.0.0.1:<端口>`，就绪信号 / 超时报错。
- [ ] 编写 `src/ui/logview.{h,cpp}`：日志页，捕获 stdout/stderr，错误行高亮，支持清空。
- [ ] 体检不通过 → 提示缺失项并提供「一键构建」（执行 install + build，构建完成后重新体检）。

## 验收标准

- 能启动 dsh 并在主页加载出 Web UI。
- 停止、重启、异常退出提示均正确。
- 日志页有进程输出；端口不通时超时报错。

## 技术注意事项

- 命令主体解析用 `QProcess::splitCommand`（Qt6 提供）或手动 `QString::split`，注意引号内空格。
- 就绪轮询要有超时（如 30–60s），避免无限等待。
- 崩溃判定：进程 `finished` 信号 + 退出码非 0 且非主动停止时，视为异常。

## 产出文件

- `src/process/preflightchecker.h/.cpp`
- `src/process/dshprocessmanager.h/.cpp`
- `src/process/readywaiter.h/.cpp`
- `src/ui/logview.h/.cpp`
