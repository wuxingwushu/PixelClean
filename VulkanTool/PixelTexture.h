#pragma once

#include "../base.h"
#include "../VulKan/image.h"
#include "../VulKan/sampler.h"
#include "../VulKan/device.h"
#include "../VulKan/commandPool.h"

namespace VulKan {

	class PixelTexture {
	public:

		PixelTexture(
			Device* device, 
			const CommandPool* commandPool, 
			const unsigned char* pixelS,
			unsigned int texWidth, 
			unsigned int texHeight, 
			unsigned int ChannelsNumber,
			Sampler* sampler
		);

		~PixelTexture();

		//上传完整图片数据到HOST缓存
		void updateBufferByMap(void* data, size_t size) { HOSTImage->updateBufferByMap(data, size); }

		//上传完整图片数据（更新到GPU）
		void ModifyImage(size_t size, void* data);
		
		//将HOST缓存上传到GPU
		void UpDataImage();

		//统一上传
		void SynchronizationUpDataImage();

		//获取图片
		[[nodiscard]] auto getImage() const { return mImage; }

		//获取图片信息
		[[nodiscard]] VkDescriptorImageInfo& getImageInfo() { return mImageInfo; }

		//获取图片HOST缓存的指针
		[[nodiscard]] void* getHOSTImagePointer() { return HOSTImage->getupdateBufferByMap(); }

		//关闭HOST缓存的指针
		void endHOSTImagePointer() { HOSTImage->endupdateBufferByMap(); }

		//获取HOST缓存
		[[nodiscard]] VkBuffer getHOSTImageBuffer() { return HOSTImage->getBuffer(); }

	private:
		Device* mDevice{ nullptr };
		Image* mImage{ nullptr };
		Buffer* HOSTImage{ nullptr };
		const CommandPool* mCommandPool{ nullptr };
		CommandBuffer* mCommandBuffer{ nullptr };
		VkDescriptorImageInfo mImageInfo{};
	};

}