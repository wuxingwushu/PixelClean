cmake_minimum_required(VERSION 3.12)
project(PixelClean)

# 查找 Vulkan SDK（自动搜索标准安装路径）
if(NOT DEFINED ENV{VULKAN_SDK})
    # Windows 常见安装路径
    if(WIN32)
        if(EXISTS "C:/VulkanSDK")
            file(GLOB VULKAN_VERSIONS "C:/VulkanSDK/*")
            list(SORT VULKAN_VERSIONS COMPARE NATURAL ORDER DESCENDING)
            list(GET VULKAN_VERSIONS 0 LATEST_VULKAN)
            set(ENV{VULKAN_SDK} "${LATEST_VULKAN}")
            message(STATUS "Auto-detected Vulkan SDK: $ENV{VULKAN_SDK}")
        endif()
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)

# MSVC编译器选项：允许匿名结构体/联合体（Microsoft扩展）+ 设置源文件编码为UTF-8
if(MSVC)
    add_compile_options(/wd4201)  # 禁用警告：非标准扩展使用了无名结构体/联合体
    add_compile_options(/utf-8)   # 设置源文件和执行字符集为UTF-8
    add_compile_options(/FS)      # 允许多个编译进程同时写入同一个PDB文件
    add_compile_options(/MP)      # 多核并行编译：单个项目内多个源文件同时编译（需配合 /FS）
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)  # Windows 宏 + GLM 全局一致配置
endif()

aux_source_directory(. DIRSRCS)
aux_source_directory(Camera CAMERA_SRCS)


if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
	message("Debug 模式")
else()
	message("Release 模式")
	# MSVC 使用 /O2，GCC/Clang 使用 -O3
	if(MSVC)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
	endif()
	# o2: 代码编译优化
	# -Ofast：在进行某些优化时，可能会牺牲一些标准兼容性。
	# Os: 优化代码大小,尽可能地缩小可执行文件的体积。
	# -s: 关闭调试信息
	# -march=native: 使用特定的 CPU 架构
endif()

# 设置 include 的引用头文件路径
include_directories(
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/Environment/glfw/include
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/Environment/libevent/include
	${CMAKE_BINARY_DIR}/Environment/libevent/include  # libevent 的 event-config.h 生成在这里
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/Environment  # zlib 源代码父目录，支持 #include "zlib/zlib.h"
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/Environment/spdlog/include
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/Environment/Soloud/include
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/Environment/imgui
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/Environment/imgui/backends
	${CMAKE_CURRENT_SOURCE_DIR}/Camera
	${CMAKE_CURRENT_SOURCE_DIR}/GOBT
)

# Vulkan SDK configuration
if(DEFINED ENV{VULKAN_SDK})
	include_directories(SYSTEM $ENV{VULKAN_SDK}/Include)
	# 根据目标架构选择库目录：Lib 为 x64，Lib32 为 x86
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(VULKAN_LIB_DIR "$ENV{VULKAN_SDK}/Lib")
		set(VULKAN_SDK_BITS "64")
	else()
		set(VULKAN_LIB_DIR "$ENV{VULKAN_SDK}/Lib32")
		set(VULKAN_SDK_BITS "32")
	endif()
	if(NOT EXISTS "${VULKAN_LIB_DIR}/vulkan-1.lib")
		message(FATAL_ERROR "Vulkan SDK 下未找到 ${VULKAN_LIB_DIR}/vulkan-1.lib。"
			"当前目标为 ${VULKAN_SDK_BITS} 位，但本机 Vulkan SDK 无对应架构库。"
			"请使用 64 位编译器环境：VS Code 选择 amd64 工具包，VS IDE 选择 x64 配置")
	endif()
	link_directories(SYSTEM ${VULKAN_LIB_DIR})
	message(STATUS "Vulkan library directory: ${VULKAN_LIB_DIR}")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/PhysicsBlock.ini ${CMAKE_CURRENT_BINARY_DIR})# 拷贝配置信息
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/Info.ini ${CMAKE_CURRENT_BINARY_DIR})# 拷贝配置信息
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/launch.vs.json ${PROJECT_SOURCE_DIR}/.vs)# 设置的DLL文件路径
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Resource ${CMAKE_CURRENT_BINARY_DIR}/Resource)# 数字资源

# 查找 Vulkan SDK 下的 glslangValidator
find_program(GLSLANG_VALIDATOR
    NAMES glslangValidator glslangValidator.exe
    HINTS $ENV{VULKAN_SDK}/Bin $ENV{VULKAN_SDK}/Bin32
    DOC "Path to glslangValidator executable from Vulkan SDK"
)

if(NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found! Please install Vulkan SDK or set VULKAN_SDK environment variable.")
endif()
message(STATUS "Found glslangValidator: ${GLSLANG_VALIDATOR}")

# 编译 GLSL shaders 为 SPIR-V
set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

# 在 CMake 配置阶段编译 shaders（每次重新配置时都会执行）
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR})
execute_process(
    COMMAND cmd /c "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compile.bat" "${SHADER_OUTPUT_DIR}" "${GLSLANG_VALIDATOR}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE SHADER_COMPILE_RESULT
)

if(NOT SHADER_COMPILE_RESULT EQUAL 0)
    message(WARNING "Shader compilation failed. Please check shaders/compile.bat")
endif()

# 同时创建自定义目标，确保构建时也会检查 shader
add_custom_target(compile_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
    COMMAND cmd /c "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compile.bat" "${SHADER_OUTPUT_DIR}" "${GLSLANG_VALIDATOR}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking and compiling GLSL shaders to SPIR-V..."
    VERBATIM
)

add_subdirectory(Environment/glfw)
add_subdirectory(Environment/libevent)
add_subdirectory(Environment/zlib)
add_subdirectory(Environment/WaveFunctionCollapse)
add_subdirectory(Environment/spdlog)
add_subdirectory(Environment/Soloud)
add_subdirectory(Environment/imgui)
add_subdirectory(Vulkan)
add_subdirectory(VulkanTool)
add_subdirectory(InterfaceUI)
add_subdirectory(BlockS)
add_subdirectory(Audio)
#add_subdirectory(SoundEffect)
add_subdirectory(NetworkTCP)
add_subdirectory(GOBT)
add_subdirectory(Character)
add_subdirectory(PhysicsBlock)
add_subdirectory(Arms)
add_subdirectory(Opcode)
add_subdirectory(GameMods)
add_subdirectory(GameMods/MazeMods)
add_subdirectory(GameMods/TankTrouble)
add_subdirectory(GameMods/UnlimitednessMapMods)
add_subdirectory(GameMods/PhysicsTest)
add_subdirectory(GameMods/SoloudTest)
add_subdirectory(GameMods/RadianceCascades)
add_subdirectory(GameMods/FruitNinja)
add_subdirectory(GameMods/WFCTest)
add_subdirectory(Tool)#文件夹里面要有代码文件，不可以是空的，会报错


add_executable(PixelClean  ${DIRSRCS} ${CAMERA_SRCS}
	resource.rc#设置exe程序图标
)

# 确保 PixelClean 依赖 shader 编译完成
add_dependencies(PixelClean compile_shaders)

# 预处理定义
#target_compile_definitions( VulKan PRIVATE _CONSOLE)
#target_compile_definitions(VulKan PRIVATE -DWIN32_LEAN_AND_MEAN)

if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
	target_link_libraries(
		#设置启动项
		PixelClean

		# 文件夹代码（按依赖顺序：依赖多的在前，被依赖的 vulkanLib 放最后）
		vulkanToolLib
		InterfaceUI
		BlockSLib
		ToolLib
		AudioEngineLib
		NetworkTCPLib
		CharacterLib
		ArmsLib
		OpcodeLib
		GameModsLib
		MazeModsLib
		TankTroubleLib
		UnlimitednessMapModsLib
		PhysicsBlockLib
		SoloudTestLib
		RadianceCascadesLib
		FruitNinjaLib
		WFCTestLib
		vulkanLib
		glfw
		libevent
		imgui

		# Soloud 音频库
		soloud

		# VulKan API
		vulkan-1.lib

		# zlib 数据压缩
		zlib

		# WaveFunctionCollapse 波函数坍缩算法
		WaveFunctionCollapse

		# SpdLog 日志库
		spdlog
	)
else()
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")#关闭控制台


	target_link_libraries(
		#设置启动项
		PixelClean 

		# 文件夹代码（按依赖顺序：依赖多的在前，被依赖的 vulkanLib 放最后）
		vulkanToolLib
		InterfaceUI 
		BlockSLib 
		ToolLib 
		AudioEngineLib
		NetworkTCPLib
		CharacterLib
		ArmsLib
		OpcodeLib
		GameModsLib
		MazeModsLib
		TankTroubleLib
		UnlimitednessMapModsLib
		PhysicsBlockLib
		SoloudTestLib
		RadianceCascadesLib
		FruitNinjaLib
		WFCTestLib
		vulkanLib
		glfw
		libevent
		imgui

		# Soloud 音频库
		soloud
	
		# VulKan API
		vulkan-1.lib 

		# zlib 数据压缩
		zlib

		# WaveFunctionCollapse 波函数坍缩算法
		WaveFunctionCollapse

		# SpdLog 日志库
		spdlog
	)
endif()



# =============================================================================
# 后置构建：把运行所需的资源复制到 exe 所在目录（shaders/Resource/ini）
# 游戏内所有资源路径都是相对路径（./shaders/ 等），依赖工作目录，
# 复制到 exe 旁边后，双击运行或任意工作目录启动都不会找不到文件
# =============================================================================
add_custom_command(TARGET PixelClean POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders $<TARGET_FILE_DIR:PixelClean>/shaders
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Resource $<TARGET_FILE_DIR:PixelClean>/Resource
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/PhysicsBlock.ini $<TARGET_FILE_DIR:PixelClean>/PhysicsBlock.ini
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/Info.ini $<TARGET_FILE_DIR:PixelClean>/Info.ini
	COMMENT "Copying shaders/Resource/ini next to executable"
	VERBATIM
)
