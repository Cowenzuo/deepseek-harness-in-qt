cmake_minimum_required(VERSION 3.21)

project(deepseek-harness-in-qt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets WebEngineWidgets Concurrent)

# 搜集 src/ 下全部源文件（CONFIGURE_DEPENDS：新增/删除文件时自动重新配置）
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

qt_add_executable(deepseek-harness-in-qt
    ${APP_SOURCES}
    resources/resources.qrc
)

# Windows 可执行文件图标资源
if(WIN32)
    target_sources(deepseek-harness-in-qt PRIVATE resources/app.rc)
    # Windows GUI 应用（无控制台窗口）；qt_add_executable 需显式指定
    # WIN32_EXECUTABLE 才会以 Windows 子系统 + 链接 qtmain（提供 WinMain 入口）。
    set_target_properties(deepseek-harness-in-qt PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_link_libraries(deepseek-harness-in-qt PRIVATE
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::Concurrent
)

# 统一以 src/ 为 include 根目录
target_include_directories(deepseek-harness-in-qt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# VS 调试运行时把 Qt bin 加入 PATH（含 QtWebEngineProcess.exe）
if(MSVC)
    # 源文件为 UTF-8（无 BOM）：显式指定 /utf-8，避免中文按系统代码页解析出错
    add_compile_options("/utf-8")
    set_target_properties(deepseek-harness-in-qt PROPERTIES
        VS_DEBUGGER_ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$(PATH)"
    )
endif()

# ---------- 测试 ----------
enable_testing()
add_subdirectory(tests)

# ---------- 安装规则 ----------
# 可执行文件装到 <prefix>/bin；macOS 的 app bundle 装到 <prefix>
install(TARGETS deepseek-harness-in-qt
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION .
)

# 生成部署脚本（Windows 用 windeployqt / macOS 用 macdeployqt），
# 随 install 自动把 Qt 运行时（含 QtWebEngineProcess.exe）拷到安装目录。
qt_generate_deploy_app_script(
    TARGET deepseek-harness-in-qt
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
