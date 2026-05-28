cmake_minimum_required(VERSION 3.22.1)

project("pixelclean")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PIXEL_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)
set(PIXEL_ENV_DIR  ${PIXEL_ROOT_DIR}/Environment)

# 查找 Vulkan SDK（自动搜索标准安装路径）
if(DEFINED ENV{VULKAN_SDK})
    set(VULKAN_SDK_PATH $ENV{VULKAN_SDK})
elseif(WIN32)
    if(EXISTS "C:/VulkanSDK")
        file(GLOB VULKAN_VERSIONS "C:/VulkanSDK/*")
        list(SORT VULKAN_VERSIONS COMPARE NATURAL ORDER DESCENDING)
        list(GET VULKAN_VERSIONS 0 LATEST_VULKAN)
        set(VULKAN_SDK_PATH "${LATEST_VULKAN}")
    endif()
endif()

if(VULKAN_SDK_PATH)
    message(STATUS "Found Vulkan SDK: ${VULKAN_SDK_PATH}")
endif()

set(PIXEL_VULKAN_DIR              ${PIXEL_ROOT_DIR}/Vulkan)
set(PIXEL_VULKANTOOL_DIR          ${PIXEL_ROOT_DIR}/VulkanTool)
set(PIXEL_TOOL_DIR                ${PIXEL_ROOT_DIR}/Tool)
set(PIXEL_BLOCKS_DIR              ${PIXEL_ROOT_DIR}/BlockS)
set(PIXEL_GAMEMODS_DIR            ${PIXEL_ROOT_DIR}/GameMods)
set(PIXEL_INTERFACEUI_DIR         ${PIXEL_ROOT_DIR}/InterfaceUI)
set(PIXEL_PHYSICS_DIR             ${PIXEL_ROOT_DIR}/Physics)
set(PIXEL_PHYSICSBLOCK_DIR        ${PIXEL_ROOT_DIR}/PhysicsBlock)
set(PIXEL_ARMS_DIR                ${PIXEL_ROOT_DIR}/Arms)
set(PIXEL_CHARACTER_DIR           ${PIXEL_ROOT_DIR}/Character)
set(PIXEL_SOUNDEFFECT_DIR         ${PIXEL_ROOT_DIR}/Audio)
set(PIXEL_NETWORKTCP_DIR          ${PIXEL_ROOT_DIR}/NetworkTCP)
set(PIXEL_OPCODE_DIR              ${PIXEL_ROOT_DIR}/Opcode)
set(PIXEL_GENERALGPU_DIR          ${PIXEL_ROOT_DIR}/GeneralCalculationGPU)
set(PIXEL_MAZEMODS_DIR            ${PIXEL_ROOT_DIR}/GameMods/MazeMods)
set(PIXEL_TANKTROUBLE_DIR         ${PIXEL_ROOT_DIR}/GameMods/TankTrouble)
set(PIXEL_UNLIMITEDNESS_DIR       ${PIXEL_ROOT_DIR}/GameMods/UnlimitednessMapMods)
set(PIXEL_PHYSICSTEST_DIR         ${PIXEL_ROOT_DIR}/GameMods/PhysicsTest)
set(PIXEL_SOLOUDTEST_DIR          ${PIXEL_ROOT_DIR}/GameMods/SoloudTest)
set(PIXEL_RADIANCE_DIR            ${PIXEL_ROOT_DIR}/GameMods/RadianceCascades)

if(ANDROID)
    set(PIXEL_PLATFORM "Android")
    message(STATUS "PixelClean target platform: Android (${ANDROID_ABI})")
elseif(WIN32)
    set(PIXEL_PLATFORM "Windows")
    message(STATUS "PixelClean target platform: Windows")
else()
    message(FATAL_ERROR "Unsupported platform.")
endif()

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384")
set(CMAKE_ANDROID_STL_TYPE c++_static)

file(GLOB_RECURSE VULKAN_SOURCES           ${PIXEL_VULKAN_DIR}/*.cpp)
file(GLOB_RECURSE VULKANTOOL_SOURCES       ${PIXEL_VULKANTOOL_DIR}/*.cpp)
file(GLOB_RECURSE TOOL_SOURCES             ${PIXEL_TOOL_DIR}/*.cpp)
file(GLOB_RECURSE BLOCKS_SOURCES           ${PIXEL_BLOCKS_DIR}/*.cpp)
file(GLOB_RECURSE GAMEMODS_SOURCES         ${PIXEL_GAMEMODS_DIR}/*.cpp)
file(GLOB_RECURSE INTERFACEUI_SOURCES      ${PIXEL_INTERFACEUI_DIR}/*.cpp)
file(GLOB_RECURSE PHYSICS_SOURCES          ${PIXEL_PHYSICS_DIR}/*.cpp)
file(GLOB_RECURSE PHYSICSBLOCK_SOURCES     ${PIXEL_PHYSICSBLOCK_DIR}/*.cpp)
file(GLOB_RECURSE ARMS_SOURCES             ${PIXEL_ARMS_DIR}/*.cpp)
file(GLOB_RECURSE CHARACTER_SOURCES        ${PIXEL_CHARACTER_DIR}/*.cpp)
file(GLOB_RECURSE SOUNDEFFECT_SOURCES      ${PIXEL_SOUNDEFFECT_DIR}/*.cpp)
file(GLOB_RECURSE NETWORKTCP_SOURCES       ${PIXEL_NETWORKTCP_DIR}/*.cpp)
file(GLOB_RECURSE OPCODE_SOURCES           ${PIXEL_OPCODE_DIR}/*.cpp)
file(GLOB_RECURSE GENERALGPU_SOURCES       ${PIXEL_GENERALGPU_DIR}/*.cpp)
file(GLOB_RECURSE MAZEMODS_SOURCES         ${PIXEL_MAZEMODS_DIR}/*.cpp)
file(GLOB_RECURSE TANKTROUBLE_SOURCES      ${PIXEL_TANKTROUBLE_DIR}/*.cpp)
file(GLOB_RECURSE UNLIMITEDNESS_SOURCES    ${PIXEL_UNLIMITEDNESS_DIR}/*.cpp)
file(GLOB_RECURSE PHYSICSTEST_SOURCES      ${PIXEL_PHYSICSTEST_DIR}/*.cpp)
file(GLOB_RECURSE SOLOUDTEST_SOURCES       ${PIXEL_SOLOUDTEST_DIR}/*.cpp)
file(GLOB_RECURSE RADIANCE_SOURCES         ${PIXEL_RADIANCE_DIR}/*.cpp)

add_library(${CMAKE_PROJECT_NAME} SHARED
    native-lib.cpp
    ${PIXEL_ROOT_DIR}/main.cpp
    ${PIXEL_ROOT_DIR}/application.cpp
    ${PIXEL_ROOT_DIR}/GlobalVariable.cpp
    ${PIXEL_ROOT_DIR}/Camera.cpp
    ${VULKAN_SOURCES}
    ${VULKANTOOL_SOURCES}
    ${TOOL_SOURCES}
    ${BLOCKS_SOURCES}
    ${GAMEMODS_SOURCES}
    ${INTERFACEUI_SOURCES}
    ${PHYSICS_SOURCES}
    ${PHYSICSBLOCK_SOURCES}
    ${ARMS_SOURCES}
    ${CHARACTER_SOURCES}
    ${SOUNDEFFECT_SOURCES}
    ${NETWORKTCP_SOURCES}
    ${OPCODE_SOURCES}
    ${GENERALGPU_SOURCES}
    ${MAZEMODS_SOURCES}
    ${TANKTROUBLE_SOURCES}
    ${UNLIMITEDNESS_SOURCES}
    ${PHYSICSTEST_SOURCES}
    ${SOLOUDTEST_SOURCES}
    ${RADIANCE_SOURCES}
)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PIXEL_ROOT_DIR}
    ${PIXEL_VULKAN_DIR}
    ${PIXEL_VULKANTOOL_DIR}
    ${PIXEL_TOOL_DIR}
    ${PIXEL_ENV_DIR}/imgui
    ${PIXEL_ENV_DIR}/imgui/backends
    ${PIXEL_ENV_DIR}/Soloud/include
    ${PIXEL_ENV_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/libevent/include
)

if(VULKAN_SDK_PATH)
    target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${VULKAN_SDK_PATH}/Include)
    message(STATUS "Added Vulkan SDK include: ${VULKAN_SDK_PATH}/Include")
endif()

if(VULKAN_SDK_PATH)
    target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${VULKAN_SDK_PATH}/Include)
    message(STATUS "Added Vulkan SDK include: ${VULKAN_SDK_PATH}/Include")
endif()

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    PIXEL_USE_VULKAN=1
    PIXEL_ANDROID=1
    ANDROID
)

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-missing-field-initializers
    -fexceptions
    -frtti
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -g -O0)
else()
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -O3 -DNDEBUG)
endif()

find_library(log_lib log)
find_library(android_lib android)
find_library(vulkan_lib vulkan)

add_subdirectory(${PIXEL_ENV_DIR}/zlib ${CMAKE_CURRENT_BINARY_DIR}/zlib)
add_subdirectory(${PIXEL_ENV_DIR}/spdlog ${CMAKE_CURRENT_BINARY_DIR}/spdlog)
add_subdirectory(${PIXEL_ENV_DIR}/imgui ${CMAKE_CURRENT_BINARY_DIR}/imgui)
add_subdirectory(${PIXEL_ENV_DIR}/Soloud ${CMAKE_CURRENT_BINARY_DIR}/soloud)
add_subdirectory(${PIXEL_ENV_DIR}/libevent ${CMAKE_CURRENT_BINARY_DIR}/libevent)

target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC
    ${log_lib}
    ${android_lib}
    ${vulkan_lib}
    zlib
    spdlog
    imgui
    soloud
    libevent
)

message(STATUS "PixelClean Android CMake configuration complete.")
message(STATUS "  C++ Standard  : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type    : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Platform      : ${PIXEL_PLATFORM}")
message(STATUS "  Root Dir      : ${PIXEL_ROOT_DIR}")