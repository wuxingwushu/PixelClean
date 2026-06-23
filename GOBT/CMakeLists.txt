cmake_minimum_required(VERSION 3.16)
project(gobot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 编译警告函数
function(enable_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive- /WX)
    else()
        target_compile_options(${target_name} PRIVATE
            -Wall -Wextra -Wpedantic -Werror
            -Wshadow -Wconversion -Wsign-conversion
        )
    endif()
endfunction()

# 主库（header-only）
add_library(gobot INTERFACE)

target_include_directories(gobot INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(gobot INTERFACE cxx_std_17)

# 测试（可选）
option(GOBOT_BUILD_TESTS "Build unit tests" OFF)
if(GOBOT_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG QUIET)
    if(GTest_FOUND)
        add_subdirectory(tests)
    else()
        message(STATUS "GoogleTest not found, skipping tests. Install gogletest or set GOBOT_BUILD_TESTS=OFF")
    endif()
endif()

# 示例（可选）
option(GOBOT_BUILD_EXAMPLES "Build examples" OFF)
if(GOBOT_BUILD_EXAMPLES)
    add_subdirectory(examples/npc_demo)
endif()
