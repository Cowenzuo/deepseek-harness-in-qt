find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(tst_core
    tst_core.cpp
    ${CMAKE_SOURCE_DIR}/src/process/proxydetector.cpp
    ${CMAKE_SOURCE_DIR}/src/process/dshprocessmanager.cpp
    ${CMAKE_SOURCE_DIR}/src/process/readywaiter.cpp
    ${CMAKE_SOURCE_DIR}/src/process/startdetached.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/appsettings.cpp
)

target_include_directories(tst_core PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_core PRIVATE Qt6::Test Qt6::Core Qt6::Network)

add_test(NAME tst_core COMMAND tst_core)
