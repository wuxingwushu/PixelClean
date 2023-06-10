#pragma once
#include <iostream>//打印
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

void Mandelbrot(GAME::VulKan::Device* device);