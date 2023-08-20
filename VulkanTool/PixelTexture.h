#pragma once

#include "../base.h"
#include "../VulKan/image.h"//����ͼƬ
#include "../VulKan/sampler.h"//����ͼƬ������
#include "../VulKan/device.h"
#include "../VulKan/commandPool.h"

namespace GAME {

	class PixelTexture {
	public:

		PixelTexture(
			VulKan::Device* device, 
			const VulKan::CommandPool* commandPool, 
			const unsigned char* pixelS,
			unsigned int texWidth, 
			unsigned int texHeight, 
			unsigned int ChannelsNumber,
			VulKan::Sampler* sampler
		);

		~PixelTexture();

		void ModifyImage(size_t size, void* data);


		[[nodiscard]] auto getImage() const { return mImage; }

		//[[nodiscard]] auto getSampler() const { return mSampler; }

		[[nodiscard]] VkDescriptorImageInfo& getImageInfo() { return mImageInfo; }

		[[nodiscard]] void* getHOSTImagePointer() { return HOSTImage->getupdateBufferByMap(); }

		[[nodiscard]] void endHOSTImagePointer() { return HOSTImage->endupdateBufferByMap(); }

		[[nodiscard]] VkBuffer getHOSTImageBuffer() { return HOSTImage->getBuffer(); }

		void UpDataImage();

	private:
		VulKan::Device* mDevice{ nullptr };
		VulKan::Image* mImage{ nullptr };
		VulKan::Buffer* HOSTImage{ nullptr };
		const VulKan::CommandPool* mCommandPool{ nullptr };
		VulKan::CommandBuffer* mCommandBuffer{ nullptr };
		VkDescriptorImageInfo mImageInfo{};
	};

}