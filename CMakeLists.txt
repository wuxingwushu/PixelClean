cmake_minimum_required(VERSION 3.12)
project(PixelClean)

# 查找并加载OpenMP模块
find_package(OpenMP REQUIRED)

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
endif()

aux_source_directory(. DIRSRCS)


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
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/Environment/SpdLog/include
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/Environment/Soloud/include
)

# Vulkan SDK configuration
if(DEFINED ENV{VULKAN_SDK})
	include_directories(SYSTEM $ENV{VULKAN_SDK}/Include)
	if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
		link_directories(SYSTEM $ENV{VULKAN_SDK}/Lib)
	else()
		link_directories(SYSTEM $ENV{VULKAN_SDK}/Lib)
	endif()
endif()

# 设置 lib 的文件路径（区分 Debug / Release）
if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
	link_directories(
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/Environment/SpdLog/lib/Debug
	)
else()
	link_directories(
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/Environment/SpdLog/lib/Release
	)
endif()


execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/PhysicsBlock.ini ${CMAKE_CURRENT_BINARY_DIR})# 拷贝配置信息
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/Info.ini ${CMAKE_CURRENT_BINARY_DIR})# 拷贝配置信息
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/launch.vs.json ${PROJECT_SOURCE_DIR}/.vs)# 设置的DLL文件路径
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}./SoundEffect/Resources ${CMAKE_CURRENT_BINARY_DIR}/Resources)# 拷贝音效
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/ImGui/Minecraft_AE.ttf ${CMAKE_CURRENT_BINARY_DIR})# 字体
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
add_subdirectory(Environment/Soloud)
add_subdirectory(Vulkan)
add_subdirectory(VulkanTool)
add_subdirectory(ImGui)
add_subdirectory(BlockS)
add_subdirectory(SoundEffect)
add_subdirectory(NetworkTCP)
add_subdirectory(Character)
add_subdirectory(Physics)
add_subdirectory(PhysicsBlock)
add_subdirectory(Arms)
add_subdirectory(GeneralCalculationGPU)
add_subdirectory(Opcode)
add_subdirectory(GameMods)
add_subdirectory(GameMods/MazeMods)
add_subdirectory(GameMods/TankTrouble)
add_subdirectory(GameMods/UnlimitednessMapMods)
add_subdirectory(GameMods/PhysicsTest)
add_subdirectory(GameMods/SoloudTest)
add_subdirectory(GameMods/RadianceCascades)
add_subdirectory(Tool)#文件夹里面要有代码文件，不可以是空的，会报错


add_executable(PixelClean  ${DIRSRCS}
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

		# 文件夹代码
		vulkanLib
		vulkanToolLib
		imguiLib
		BlockSLib
		ToolLib
		SoundEffectLib
		NetworkTCPLib
		CharacterLib
		PhysicsLib
		ArmsLib
		GeneralCalculationGPULib
		OpcodeLib
		GameModsLib
		MazeModsLib
		TankTroubleLib
		UnlimitednessMapModsLib
		PhysicsBlockLib
		SoloudTestLib
		RadianceCascadesLib
		glfw
		libevent

		# Soloud 音频库
		soloud

		# VulKan API
		vulkan-1.lib

		# zlib 数据压缩
		zlib

		# SpdLog 日志库
		spdlogd.lib

		# 多线程
		OpenMP::OpenMP_CXX
	)
else()
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")#关闭控制台


	target_link_libraries(
		#设置启动项
		PixelClean 

		# 文件夹代码
		vulkanLib
		vulkanToolLib
		imguiLib 
		BlockSLib 
		ToolLib 
		SoundEffectLib
		NetworkTCPLib
		CharacterLib
		PhysicsLib
		ArmsLib
		GeneralCalculationGPULib
		OpcodeLib
		GameModsLib
		MazeModsLib
		TankTroubleLib
		UnlimitednessMapModsLib
		PhysicsBlockLib
		SoloudTestLib
		RadianceCascadesLib
		glfw
		libevent

		# Soloud 音频库
		soloud
	
		# VulKan API
		vulkan-1.lib 

		# zlib 数据压缩
		zlib

		# SpdLog 日志库
		spdlog.lib

		# 多线程
		OpenMP::OpenMP_CXX
	)
endif()


