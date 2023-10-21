cmake_minimum_required(VERSION 3.12)
project(PixelClean)

# 查找并加载OpenMP模块
find_package(OpenMP REQUIRED)

set(CMAKE_CXX_STANDARD 17)

aux_source_directory(. DIRSRCS)


# add_custom_command(
#     OUTPUT ./GeneralCalculationGPU/WarfareMist.spv
#     COMMAND ./GeneralCalculationGPU/compile.bat
#     DEPENDS ./glslangValidator.exe
# )

if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
	message("Debug 模式")
else()
	message("Release 模式")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
	# o2: 代码编译优化
	# -Ofast：在进行某些优化时，可能会牺牲一些标准兼容性。
	# Os: 优化代码大小,尽可能地缩小可执行文件的体积。
	# -s: 关闭调试信息
	# -march=native: 使用特定的 CPU 架构
endif()

# 设置 include 的引用头文件路径
include_directories(
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/GLFW/include
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/VulKan/Include
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/Soloud/include
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/libevent/include
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/SpdLog/include
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/zlib/include
)

# 设置 lib 的文件路径（区分 Debug / Release）
if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
	link_directories(
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/GLFW/lib
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/VulKan/Lib
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/Soloud/lib/Debug
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/libevent/debug/lib
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/SpdLog/lib/Debug
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/zlib/lib/Debug
	)
else()
	link_directories(
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/GLFW/lib
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/VulKan/Lib
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/Soloud/lib/Release
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/libevent/lib
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/SpdLog/lib/Release
		SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}./Environment/zlib/lib/Release
	)
endif()



if(CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
	execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}./Environment/Soloud/bin/Debug ${CMAKE_CURRENT_BINARY_DIR})
	execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}./Environment/libevent/debug/bin ${CMAKE_CURRENT_BINARY_DIR})
	execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}./Environment/zlib/bin/Debug ${CMAKE_CURRENT_BINARY_DIR})
else()
	execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}./Environment/Soloud/bin/Release ${CMAKE_CURRENT_BINARY_DIR})
	execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}./Environment/libevent/bin ${CMAKE_CURRENT_BINARY_DIR})
	execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}./Environment/zlib/bin/Release ${CMAKE_CURRENT_BINARY_DIR})
endif()

execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/Info.ini ${CMAKE_CURRENT_BINARY_DIR})# 拷贝配置信息
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/launch.vs.json ${PROJECT_SOURCE_DIR}/.vs)# 设置的DLL文件路径
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/shaders/Spv ${CMAKE_CURRENT_BINARY_DIR}/shaders) # 拷贝shaders
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}./SoundEffect/Resources ${CMAKE_CURRENT_BINARY_DIR}/Resources)# 拷贝音效
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Labyrinth/Texture ${CMAKE_CURRENT_BINARY_DIR}/Texture)# 测试图片
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/ImGui/Minecraft_AE.ttf ${CMAKE_CURRENT_BINARY_DIR})# 字体

add_subdirectory(Vulkan)
add_subdirectory(VulkanTool)
add_subdirectory(ImGui)
add_subdirectory(BlockS)
add_subdirectory(SoundEffect)
add_subdirectory(NetworkTCP)
add_subdirectory(Character)
add_subdirectory(Physics)
add_subdirectory(Labyrinth)
add_subdirectory(Arms)
add_subdirectory(GeneralCalculationGPU)
add_subdirectory(Opcode)
add_subdirectory(GameMods)
add_subdirectory(Tool)#文件夹里面要有代码文件，不可以是空的，会报错


add_executable(PixelClean  ${DIRSRCS}
	resource.rc#设置exe程序图标
)

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
		LabyrinthLib
		ArmsLib
		GeneralCalculationGPULib
		OpcodeLib
		GameModsLib
	
		# VulKan API
		vulkan-1.lib 
		
		#跨平台窗口
		glfw3.lib
	
		#bug 日志
		spdlogd.lib
	
		#音效
		soloud.lib
		SDL2main.lib
		SDL2.lib
	
		#网络协议
		event.lib
		event_core.lib
		event_extra.lib

		# zlib 数据压缩
		zlibd.lib

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
		LabyrinthLib
		ArmsLib
		GeneralCalculationGPULib
		OpcodeLib
		GameModsLib
	
		# VulKan API
		vulkan-1.lib 
		
		#跨平台窗口
		glfw3.lib
	
		#bug 日志
		spdlog.lib
	
		#音效
		soloud.lib
		SDL2main.lib
		SDL2.lib
	
		#网络协议
		event.lib
		event_core.lib
		event_extra.lib

		# zlib 数据压缩
		zlib.lib

		# 多线程
		OpenMP::OpenMP_CXX
	)
endif()


