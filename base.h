#pragma once

//#define _CRTDBG_MAP_ALLOC
//_CrtDumpMemoryLeaks()
//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

/*
#ifdef _DEBUG
#define DEBUG_CLIENTBLOCK   new( _CLIENT_BLOCK, __FILE__, __LINE__)
#else
#define DEBUG_CLIENTBLOCK
#endif

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

#define new DEBUG_CLIENTBLOCK
#endif


//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);//_CrtDumpMemoryLeaks();
//_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
*/



#include <iostream>//打印
#include <vector>//动态数组
#include <array>//静态数组
#include <map>
#include <string>//字符串
#include <optional>
#include <set>
#include <fstream>
#include <thread>//多线程
#if defined(_WIN32)
#define _WINSOCKAPI_
#include <Windows.h>
#pragma comment(lib, "ws2_32.lib")
#elif defined(__ANDROID__)
#include <android/log.h>
#endif

#if defined(_WIN32)
//#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>//跨平台接口
#include <vulkan/vulkan.h>//VulKan API
#elif defined(__ANDROID__)
#include <vulkan/vulkan.h>
#define GLFW_PRESS   1
#define GLFW_RELEASE 0
#endif


//#define VLD_FORCE_ENABLE //VLD_FORCE_ENABLE宏定义是为了Release版本也能生成报告
//#include "vld.h"//内存泄露检测


#include "FilePath.h"//资源路径


#include "Tool/Tool.h"// 检查： FPS , 耗时

#include "Tool/Iterator.h"

//矩阵计算库
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
