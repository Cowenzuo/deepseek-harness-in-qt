# Phase 1 — 工程骨架

## 目标

搭起可编译运行的空壳：主窗口显示，主页的 `QWebEngineView` 能加载任意 URL，为后续功能打底。

## 前置条件

- 已安装 Qt 6.x（含 QtWebEngine / QtWebEngineWidgets 模块）与 VS2022。
- CMake ≥ 3.21。

## 任务清单

- [ ] 编写 `CMakeLists.txt`：`CMAKE_PREFIX_PATH` 指向 Qt6，`find_package(Qt6 COMPONENTS Widgets WebEngineWidgets)`，`qt_add_executable`，启用 AUTOMOC。
- [ ] 编写 `src/main.cpp`：`QApplication` + `MainWindow` 并显示。
- [ ] 编写 `src/mainwindow.{h,cpp}`：`QMainWindow` + 顶部菜单栏骨架（文件/仓库/更新/设置/帮助，先占位）+ `QStackedWidget`（主页、日志页两个占位页）。
- [ ] 主页放 `QWebEngineView`，加载测试 URL（本地 HTML 或 `http://127.0.0.1` 均可）。
- [ ] 配置 `VS_DEBUGGER_ENVIRONMENT` 附加 Qt6 `bin/` 目录（供 VS 调试时找到 Qt DLL）。
- [ ] 编译通过，运行看到窗口且 webview 成功加载页面。

## 验收标准

- 使用 CMake 生成、编译零错误。
- 程序启动显示主窗口，菜单栏可见。
- webview 能加载并渲染测试页面（非空白）。

## 技术注意事项

- Qt6 WebEngine 运行时依赖 `QtWebEngineProcess.exe`，调试运行时确保其与程序同目录或 Qt `bin/` 在环境里。
- Windows 下若用 Debug 版 Qt，WebEngine 可能因沙箱限制报错，必要时设置 `QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox` 排查（仅调试）。
- 不要引入任何 dsh 相关逻辑，本阶段只验证 webview 通路。

## 产出文件

- `CMakeLists.txt`
- `src/main.cpp`
- `src/mainwindow.h`
- `src/mainwindow.cpp`
