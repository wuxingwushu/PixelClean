#pragma once
#include "window.h"
#include "windowSurface.h"
#include "renderPass.h"
#include "image.h"
#include "commandPool.h"

namespace VulKan {

	struct SwapChainSupportInfo {
		VkSurfaceCapabilitiesKHR mCapabilities;
		std::vector<VkSurfaceFormatKHR> mFormats;
		std::vector<VkPresentModeKHR> mPresentModes;
	};

	//创建一张MutiSample的图片，并且加入到FrameBuffer里面
	class SwapChain {
	public:
		SwapChain(
			Device* device, 
			Window* window, 
			WindowSurface* surface,
			CommandPool* commandPool
		);

		void StructureSwapChain();

		~SwapChain();

		//查找交换链支持信息
		SwapChainSupportInfo querySwapChainSupportInfo();
		//选择表面格式
		VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
		//选择表面呈现模式
		VkPresentModeKHR chooseSurfacePresentMode(const std::vector<VkPresentModeKHR> &availablePresenstModes);
		//选择范围
		VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR & capabilities);

		void createFrameBuffers(const RenderPass* renderPass);

	public:
		[[nodiscard]] inline VkFormat getFormat() const noexcept { return mSwapChainFormat; }

		//获取GPU画布的数量
		[[nodiscard]] inline uint32_t getImageCount() const noexcept { return mImageCount; }

		[[nodiscard]] inline VkSwapchainKHR getSwapChain() const noexcept { return mSwapChain; }

		//获得是哪一个GPU画布
		[[nodiscard]] inline VkFramebuffer getFrameBuffer(const int index) const { return mSwapChainFrameBuffers[index]; }

		//获得屏幕的长宽
		[[nodiscard]] inline VkExtent2D getExtent() const noexcept { return mSwapChainExtent; }

		[[nodiscard]] inline VkImageView getImageView(const int index) const { return mSwapChainImageViews[index]; }

		//设备不支持多重采样时 mMutiSampleImages[index] 为空指针，此处必须返回 VK_NULL_HANDLE，
		//由调用方（创建 FrameBuffer 时）决定是否把它算进附件数组，否则会直接空指针崩溃。
		[[nodiscard]] inline VkImageView getMutiSampleImageView(const int index) const {
			return mMutiSampleImages[index] != nullptr ? mMutiSampleImages[index]->getImageView() : VK_NULL_HANDLE;
		}

		//是否存在多重采样中间图（即设备是否支持 MSAA）
		[[nodiscard]] inline bool hasMutiSampleImages() const noexcept {
			return !mMutiSampleImages.empty() && mMutiSampleImages[0] != nullptr;
		}

		[[nodiscard]] inline VkImageView getDepthImageView(const int index) const { return mDepthImages[index]->getImageView(); }

	private:
		//创建imageView
		VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels = 1);

	private:
		VkSwapchainKHR mSwapChain{ VK_NULL_HANDLE };

		VkFormat  mSwapChainFormat;
		VkExtent2D mSwapChainExtent;
		uint32_t mImageCount{ 0 };

		//vkimage由swapchain创建，销毁也要交给swapchain
		std::vector<VkImage> mSwapChainImages{};

		//对图像的管理器。管理框架
		std::vector<VkImageView> mSwapChainImageViews{};

		std::vector<VkFramebuffer> mSwapChainFrameBuffers{};

		//深度图片
		std::vector<Image*> mDepthImages{};

		//多重采样，中间图片
		std::vector<Image*> mMutiSampleImages{};

		Device* mDevice{ nullptr };
		Window* mWindow{ nullptr };
		WindowSurface* mSurface{ nullptr };
		CommandPool* mCommandPool{ nullptr };
	};
}