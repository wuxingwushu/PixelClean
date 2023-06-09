#pragma once
#include <iostream>//打印
#include <VUH/vuh.h>
#include "../Vulkan/instance.h"
#include "../Vulkan/device.h"
#include "../Vulkan/Window.h"
#include "../Vulkan/windowSurface.h"
#include "../Vulkan/swapChain.h"
#include "../Vulkan/shader.h"
#include "../Vulkan/pipeline.h"
#include "../Vulkan/renderPass.h"
#include "../Vulkan/commandPool.h"
#include "../VulKan/commandBuffer.h"
#include "lodepng.h"

uint32_t* readFile(uint32_t& length, const char* filename);

void jishuan();

void GetWarfareMist(int* y, unsigned int Size_x, unsigned int Size_y, int wanjia_x, int wanjia_y);

void Mandelbrot(GAME::VulKan::Device* device);