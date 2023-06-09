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
#define _WINSOCKAPI_
#include <Windows.h>
#pragma comment(lib, "ws2_32.lib")

//#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>//跨平台接口
#include <vulkan/vulkan.h>//VulKan API



//#define VMA_DEBUG_MARGIN 16//边距（Margins）https://blog.csdn.net/weixin_50523841/article/details/122506850
#include "vk_mem_alloc.h"//内存分配器，宏声明 放在了 device.cpp 当中去了，引用的时候要放在CPP当中用要不然会报错（反复定义）

//#define VLD_FORCE_ENABLE //VLD_FORCE_ENABLE宏定义是为了Release版本也能生成报告
//#include "vld.h"//内存泄露检测


#include "FilePath.h"//资源路径


#include "Tool/Tool.h"// 检查： FPS , 耗时



//矩阵计算库
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>




//开启的测试模式
const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"//测试类型
};


struct VPMatrices {
	glm::mat4 mViewMatrix;
	glm::mat4 mProjectionMatrix;

	VPMatrices() {
		mViewMatrix = glm::mat4(1.0f);
		mProjectionMatrix = glm::mat4(1.0f);
	}
};

struct ObjectUniform {
	glm::mat4 mModelMatrix;

	ObjectUniform() {
		mModelMatrix = glm::mat4(1.0f);
	}
};


