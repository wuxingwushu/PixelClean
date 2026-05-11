cmake_minimum_required(VERSION 3.12)
project(SoloudLib)

# Soloud 公共源码文件
file(GLOB SOLOUD_CORE src/core/*.cpp)
file(GLOB SOLOUD_AUDIOSOURCE src/audiosource/*.cpp)
file(GLOB_RECURSE SOLOUD_AUDIOSOURCE_SUB src/audiosource/*/*.cpp)
file(GLOB SOLOUD_FILTER src/filter/*.cpp)

# C 语言源文件（stb_vorbis.c 等）
file(GLOB_RECURSE SOLOUD_C_SOURCES src/audiosource/*/*.c)

if(WIN32)
    # Windows: 使用 WASAPI backend
    file(GLOB SOLOUD_BACKEND src/backend/wasapi/*.cpp)
    set(SOLOUD_PLATFORM_DEFS WITH_WASAPI)
    set(SOLOUD_PLATFORM_LIBS ole32 ksuser)
elseif(ANDROID)
    # Android: 使用 OpenSL ES backend
    file(GLOB SOLOUD_BACKEND src/backend/opensles/*.cpp)
    set(SOLOUD_PLATFORM_DEFS WITH_OPENSLES)
    set(SOLOUD_PLATFORM_LIBS OpenSLES)
else()
    # 其他平台: 使用 null backend 作为兜底
    file(GLOB SOLOUD_BACKEND src/backend/null/*.cpp)
    set(SOLOUD_PLATFORM_DEFS WITH_NULL)
    set(SOLOUD_PLATFORM_LIBS "")
endif()

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

target_compile_definitions(soloud PRIVATE ${SOLOUD_PLATFORM_DEFS})

if(SOLOUD_PLATFORM_LIBS)
    target_link_libraries(soloud PUBLIC ${SOLOUD_PLATFORM_LIBS})
endif()
