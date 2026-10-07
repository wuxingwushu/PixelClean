cmake_minimum_required(VERSION 3.16)
project(gobot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

function(enable_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        )
    endif()
endfunction()

# 主库（header-only）：全部实现在 gobot/ 下的 6 个头文件中
add_library(gobot INTERFACE)

target_include_directories(gobot INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(gobot INTERFACE cxx_std_17)

# 行为回归检查（可选）：verify/new_test.cpp 的输出需与 verify/baseline.txt 逐行一致
option(GOBOT_BUILD_VERIFY "Build gobot behavior regression check" OFF)
if(GOBOT_BUILD_VERIFY)
    add_executable(gobot_verify verify/new_test.cpp)
    target_link_libraries(gobot_verify PRIVATE gobot)
    enable_warnings(gobot_verify)
endif()
