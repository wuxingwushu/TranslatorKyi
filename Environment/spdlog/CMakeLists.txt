cmake_minimum_required(VERSION 3.12)
project(spdlog LANGUAGES CXX)

# spdlog 源代码文件
set(SPDLOG_SOURCES
    src/spdlog.cpp
    src/stdout_sinks.cpp
    src/color_sinks.cpp
    src/file_sinks.cpp
    src/async.cpp
    src/cfg.cpp
    src/bundled_fmtlib_format.cpp
)

# 创建静态库
add_library(spdlog STATIC ${SPDLOG_SOURCES})

# 设置包含目录
target_include_directories(spdlog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 编译定义：启用编译库模式（非header-only）
target_compile_definitions(spdlog PUBLIC SPDLOG_COMPILED_LIB)

# 编译定义
target_compile_definitions(spdlog PRIVATE -D_CRT_SECURE_NO_DEPRECATE)

# 链接线程库
find_package(Threads REQUIRED)
target_link_libraries(spdlog PUBLIC Threads::Threads)

# MSVC 编译器选项
if(MSVC)
    target_compile_options(spdlog PRIVATE "/Zc:__cplusplus")
endif()

# 根据构建类型设置输出名称
if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
    set_target_properties(spdlog PROPERTIES OUTPUT_NAME "spdlogd")
else()
    set_target_properties(spdlog PROPERTIES OUTPUT_NAME "spdlog")
endif()
