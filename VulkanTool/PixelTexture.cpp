#include "PixelTexture.h"

namespace GAME {
	PixelTexture::PixelTexture(
		VulKan::Device* device, 
		const VulKan::CommandPool* commandPool, 
		const unsigned char* pixelS,
		unsigned int texWidth, 
		unsigned int texHeight, 
		unsigned int ChannelsNumber,
		VulKan::Sampler* sampler
	) {
		mDevice = device;
		mCommandPool = commandPool;
		mCommandBuffer = new VulKan::CommandBuffer(mDevice, mCommandPool);

		HOSTImage = new VulKan::Buffer(
			mDevice, texWidth * texHeight * ChannelsNumber,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);

		VkFormat SRGB = VK_FORMAT_R8G8B8A8_SRGB;


		switch (ChannelsNumber)
		{
		case 1:
			SRGB = VK_FORMAT_R8_SRGB;
			break;
		case 2:
			SRGB = VK_FORMAT_R8G8_SRGB;
			break;
		case 3:
			SRGB = VK_FORMAT_R8G8B8_SRGB;
			break;
		case 4:
			SRGB = VK_FORMAT_R8G8B8A8_SRGB;
			break;
		}
		

		mImage = new VulKan::Image(
			mDevice, texWidth, texHeight,
			SRGB,
			VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		
		if (pixelS != nullptr) {
			HOSTImage->updateBufferByMap((void*)pixelS, texWidth * texHeight * ChannelsNumber);
		}

		UpDataImage();


		mImageInfo.imageLayout = mImage->getLayout();
		mImageInfo.imageView = mImage->getImageView();
		mImageInfo.sampler = sampler->getSampler();
	}

	PixelTexture::~PixelTexture() {
		delete mImage;
		delete mCommandBuffer;
		delete HOSTImage;
	}

	void PixelTexture::UpDataImage() {
		VkImageSubresourceRange region{};
		region.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		region.baseArrayLayer = 0;
		region.layerCount = 1;

		region.baseMipLevel = 0;
		region.levelCount = 1;

		mImage->setImageLayout(
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			region,
			mCommandPool
		);

		mCommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		mCommandBuffer->copyBufferToImage(HOSTImage->getBuffer(), mImage->getImage(), mImage->getLayout(), mImage->getWidth(), mImage->getHeight());
		mCommandBuffer->end();
		mCommandBuffer->submitSync(mDevice->getGraphicQueue(), VK_NULL_HANDLE);

		mImage->setImageLayout(
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			region,
			mCommandPool
		);
	}

	void PixelTexture::ModifyImage(size_t size, void* data) {
		HOSTImage->updateBufferByMap(data, size);
		UpDataImage();
	}
}