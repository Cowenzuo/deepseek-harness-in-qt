cmake_minimum_required(VERSION 3.21)

project(deepseek-harness-in-qt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets WebEngineWidgets Concurrent)

qt_add_executable(deepseek-harness-in-qt
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/settings/appsettings.cpp
    src/settings/appsettings.h
    src/settings/settingsdialog.cpp
    src/settings/settingsdialog.h
    src/process/preflightchecker.cpp
    src/process/preflightchecker.h
    src/process/environmentchecker.cpp
    src/process/environmentchecker.h
    src/process/proxydetector.cpp
    src/process/proxydetector.h
    src/process/startdetached.cpp
    src/process/readywaiter.cpp
    src/process/readywaiter.h
    src/process/dshprocessmanager.cpp
    src/process/dshprocessmanager.h
    src/ui/logview.cpp
    src/ui/logview.h
    src/ui/homepage.cpp
    src/ui/homepage.h
    src/ui/startuppage.cpp
    src/ui/startuppage.h
    src/ui/setuppage.cpp
    src/ui/setuppage.h
    src/ui/errorpage.cpp
    src/ui/errorpage.h
    src/git/gitclient.cpp
    src/git/gitclient.h
    src/ui/branchdialog.cpp
    src/ui/branchdialog.h
    src/ui/commitdialog.cpp
    src/ui/commitdialog.h
    src/update/updatemanager.cpp
    src/update/updatemanager.h
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
    set_target_properties(deepseek-harness-in-qt PROPERTIES
        VS_DEBUGGER_ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$(PATH)"
    )
endif()
