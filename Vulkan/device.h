#pragma once
//#define VMA_DEBUG_MARGIN 16//边距（Margins）https://blog.csdn.net/weixin_50523841/article/details/122506850
#include <vma/vk_mem_alloc.h>
#include "instance.h"
#include "windowSurface.h"
#include <optional>


namespace VulKan {

	const std::vector<const char*> deviceRequiredExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_MAINTENANCE1_EXTENSION_NAME,//开启 VulKan 坐标系扩展
		//VK_NV_FRAMEBUFFER_MIXED_SAMPLES_EXTENSION_NAME
	};

	class Device {
	public:
		Device(Instance* instance, WindowSurface* surface);

		~Device();

		//在所以设备中选择分数最高的
		void pickPhysicalDevice();

		//给设备评分
		int rateDevice(VkPhysicalDevice device);

		//判断设备是否符合要求
		bool isDeviceSuitable(VkPhysicalDevice device);

		//初始化队列族
		void initQueueFamilies(VkPhysicalDevice device);

		//获取逻辑设备，创建对应引用ID
		void createLogicalDevice();

		//判断设备是否完整
		bool isQueueFamilyComplete();

		VkSampleCountFlagBits getMaxUsableSampleCount();

		[[nodiscard]] inline VmaAllocator getAllocator() const noexcept { return mAllocator; }//获取内存分配器

		[[nodiscard]] inline VkDevice getDevice() const noexcept { return mDevice; }
		[[nodiscard]] inline VkPhysicalDevice getPhysicalDevice() const noexcept { return mPhysicalDevice; }

		[[nodiscard]] inline std::optional<uint32_t> getGraphicQueueFamily() const noexcept { return mGraphicQueueFamily; }
		[[nodiscard]] inline std::optional<uint32_t> getPresentQueueFamily() const noexcept { return mPresentQueueFamily; }

		[[nodiscard]] inline VkQueue getGraphicQueue() const noexcept { return mGraphicQueue; }
		[[nodiscard]] inline VkQueue getPresentQueue() const noexcept { return mPresentQueue; }

	private:
		VkPhysicalDevice mPhysicalDevice{ VK_NULL_HANDLE };//获得的详细设备信息
		Instance* mInstance{ nullptr };
		WindowSurface* mSurface{ nullptr };

		//***  Vulkan 将诸如绘制指令、内存操作提交到VkQueue 中，进行异步执行。

		//存储当前渲染任务队列族的id
		std::optional<uint32_t> mGraphicQueueFamily;
		VkQueue	mGraphicQueue{ VK_NULL_HANDLE };

		std::optional<uint32_t> mPresentQueueFamily;
		VkQueue mPresentQueue{ VK_NULL_HANDLE };

		//逻辑设备
		VkDevice mDevice{ VK_NULL_HANDLE };

		//创建的内存分配器
		VmaAllocator mAllocator{ VK_NULL_HANDLE };
		
	};
}