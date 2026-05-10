cmake_minimum_required(VERSION 3.22.1)

project("pixelclean")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384")

set(PIXEL_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)
set(PIXEL_ENV_DIR  ${PIXEL_ROOT_DIR}/Environment)

set(PIXEL_VULKAN_DIR     ${PIXEL_ROOT_DIR}/Vulkan)
set(PIXEL_VULKANTOOL_DIR ${PIXEL_ROOT_DIR}/VulkanTool)
set(PIXEL_TOOL_DIR       ${PIXEL_ROOT_DIR}/Tool)

if(ANDROID)
    set(PIXEL_PLATFORM "Android")
    set(PIXEL_USE_VULKAN ON)
    set(PIXEL_USE_GLFW OFF)
    set(PIXEL_HAS_GLFW 0)
    set(CMAKE_ANDROID_STL_TYPE c++_static)
    message(STATUS "PixelClean target platform: Android (${ANDROID_ABI})  STL: static")
elseif(WIN32)
    set(PIXEL_PLATFORM "Windows")
    set(PIXEL_USE_VULKAN ON)
    set(PIXEL_USE_GLFW ON)
    set(PIXEL_HAS_GLFW 1)
    message(STATUS "PixelClean target platform: Windows (${CMAKE_GENERATOR_PLATFORM})")
else()
    message(FATAL_ERROR "Unsupported platform. Only Windows and Android are supported.")
endif()

set(PLATFORM_DETECTION_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PLATFORM_DETECTION_DIR}/Platform)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Platform/PlatformDetection.h.in
    ${PLATFORM_DETECTION_DIR}/Platform/PlatformDetection.h
    @ONLY
)

file(GLOB VULKAN_SOURCES
    ${PIXEL_VULKAN_DIR}/*.cpp
)
file(GLOB VULKANTOOL_SOURCES
    ${PIXEL_VULKANTOOL_DIR}/*.cpp
)

if(ANDROID)
    set(VULKAN_SOURCES "")
    set(VULKANTOOL_SOURCES "")
endif()

add_library(${CMAKE_PROJECT_NAME} SHARED
    native-lib.cpp
    VulkanApp.cpp
    ${VULKAN_SOURCES}
    ${VULKANTOOL_SOURCES}
    ${PIXEL_TOOL_DIR}/log.cpp
    ${PIXEL_ENV_DIR}/imgui/imgui.cpp
    ${PIXEL_ENV_DIR}/imgui/imgui_draw.cpp
    ${PIXEL_ENV_DIR}/imgui/imgui_tables.cpp
    ${PIXEL_ENV_DIR}/imgui/imgui_widgets.cpp
    ${PIXEL_ENV_DIR}/imgui/imgui_demo.cpp
    ${PIXEL_ENV_DIR}/imgui/backends/imgui_impl_vulkan.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PLATFORM_DETECTION_DIR}
    ${PIXEL_ROOT_DIR}
    ${PIXEL_VULKAN_DIR}
    ${PIXEL_VULKANTOOL_DIR}
    ${PIXEL_TOOL_DIR}
    ${PIXEL_ENV_DIR}/imgui
    ${PIXEL_ENV_DIR}/imgui/backends
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    PIXEL_USE_VULKAN=1
)

if(ANDROID)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -fexceptions
        -frtti
    )

    target_include_directories(${CMAKE_PROJECT_NAME} BEFORE PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Platform/Android/glfw_stub
    )

    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        PIXEL_ANDROID=1
        ANDROID
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -g -O0)
    else()
        target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -O3 -DNDEBUG)
    endif()

    find_library(log_lib log)
    find_library(android_lib android)
    find_library(vulkan_lib vulkan)

    target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC
        ${log_lib}
        ${android_lib}
        ${vulkan_lib}
    )

elseif(WIN32)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
        /W4
        /WX-
        /utf-8
        /MP
        /permissive-
        /Zc:preprocessor
    )

    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    find_package(Vulkan REQUIRED)
    if(Vulkan_FOUND)
        target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC ${Vulkan_LIBRARIES})
    endif()

    find_package(glfw3 CONFIG QUIET)
    if(glfw3_FOUND)
        target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC glfw)
        target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE PIXEL_HAS_GLFW=1)
    endif()
endif()

message(STATUS "PixelClean CMake configuration complete.")
message(STATUS "  C++ Standard  : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type    : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Platform      : ${PIXEL_PLATFORM}")
message(STATUS "  Vulkan Dir    : ${PIXEL_VULKAN_DIR}")
message(STATUS "  VulkanTool Dir: ${PIXEL_VULKANTOOL_DIR}")
message(STATUS "  Tool Dir      : ${PIXEL_TOOL_DIR}")