cmake_minimum_required(VERSION 3.12)
project(SoloudLib)

# Soloud 源码文件
file(GLOB SOLOUD_CORE src/core/*.cpp)
file(GLOB SOLOUD_AUDIOSOURCE src/audiosource/*.cpp)
file(GLOB_RECURSE SOLOUD_AUDIOSOURCE_SUB src/audiosource/*/*.cpp)
file(GLOB SOLOUD_FILTER src/filter/*.cpp)

# 使用 WASAPI backend (Windows 推荐)
file(GLOB SOLOUD_BACKEND src/backend/wasapi/*.cpp)

# C 语言源文件（stb_vorbis.c 等）
file(GLOB_RECURSE SOLOUD_C_SOURCES src/audiosource/*/*.c)

set(SOLOUD_SOURCES
    ${SOLOUD_CORE}
    ${SOLOUD_AUDIOSOURCE}
    ${SOLOUD_AUDIOSOURCE_SUB}
    ${SOLOUD_FILTER}
    ${SOLOUD_BACKEND}
    ${SOLOUD_C_SOURCES}
)

add_library(soloud STATIC ${SOLOUD_SOURCES})

target_include_directories(soloud PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 定义使用的 backend
target_compile_definitions(soloud PRIVATE WITH_WASAPI)

# WASAPI 需要的系统库
target_link_libraries(soloud PUBLIC ole32 ksuser)
