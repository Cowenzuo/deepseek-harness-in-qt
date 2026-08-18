# Phase 1 — 工程骨架

> 状态：✅ **已完成**（对应 commit `7e01472` / `9a5a4ad`）
> 产出与早期设计有差异，本节为**实际实现**的定稿描述。

## 目标

搭起可编译运行的壳：主窗口显示，主页的 `QWebEngineView` 能加载任意 URL，为后续功能打底。

## 前置条件

- 已安装 Qt 6.x（含 QtWebEngineWidgets 模块）与 VS2022+。
- CMake ≥ 3.21。

## 任务清单（实际结果）

- [x] `CMakeLists.txt`：`find_package(Qt6 COMPONENTS Widgets WebEngineWidgets Concurrent)`、`qt_add_executable`、AUTOMOC/AUTORCC/AUTOUIC、`WIN32_EXECUTABLE`（Windows GUI 无控制台）、`VS_DEBUGGER_ENVIRONMENT` 附加 Qt bin、`qt_generate_deploy_app_script` 部署。
- [x] `src/main.cpp`：`QApplication` + `MainWindow`；**QApplication 创建前**设置 `AA_ShareOpenGLContexts` 与 `QQuickWindow::setGraphicsApi(OpenGL)`（统一渲染后端防白闪）；Fusion 深色 palette；qDebug 落 `config/debug.log`；窗口图标 `:/app.ico`。
- [x] `src/mainwindow.{h,cpp}`：`QMainWindow`，中央容器 = `QStackedWidget`（启动/引导/错误/日志页）与 `HomePage`（webview）**平级叠放**，切页用 raise/lower、webview 常驻渲染（防切页白闪）；状态栏 = 左侧「>」设置按钮 + 右侧状态 label。
- [x] `src/ui/homepage.{h,cpp}`：webview 主页，窗口显示前同步创建；`loadFinished` 后**正面锚点**就绪判定（输入区 `[data-composer-card]` + 会话区 `[data-conversation-scroll]` 均挂载 → `pageReady`）；单周期 10s 未命中自愈 reload，总 60s 超时 `pageFailed`；调试用页面存档 `config/webview-N.html`。
- [x] `src/ui/startuppage.{h,cpp}`：启动加载页（不确定进度条 + 状态文字），首帧显示。
- [x] 编译通过（preset `msvc2022`，VS 2026 x64），`cmake --install` 部署到 `install/`（windeployqt 拷全量 Qt 运行时）。

## 验收标准（结果）

- CMake 生成、编译零错误；Debug/Release 均可构建部署。
- 程序启动显示主窗口，首帧为启动加载页，深色无白闪（OpenGL 渲染后端统一 + 共享上下文 + webview 常驻渲染）。
- webview 可加载并渲染页面；页面就绪后 raise 显示主页。

## 与早期设计的差异

| 早期设计 | 实际实现 |
| --- | --- |
| 顶部菜单栏骨架（文件/仓库/更新/设置/帮助占位） | 无菜单栏；功能并入设置弹窗与状态栏按钮 |
| `QStackedWidget`（主页、日志页两个占位页） | 四页（启动/引导/错误/日志）+ webview 平级叠放，raise/lower 切换 |
| webview 直接加载测试 URL | 主页带正面锚点就绪判定 + 自愈 reload 机制 |

## 产出文件

- `CMakeLists.txt`、`CMakePresets.json`
- `resources/resources.qrc`、`resources/app.rc`、`resources/app.ico`
- `src/main.cpp`
- `src/mainwindow.h`、`src/mainwindow.cpp`
- `src/ui/homepage.h`、`src/ui/homepage.cpp`
- `src/ui/startuppage.h`、`src/ui/startuppage.cpp`
