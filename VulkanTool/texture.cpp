#include "texture.h"

#define STB_IMAGE_IMPLEMENTATION//stb_image.h �ĺ궨��
#include "stb_image.h"

namespace VulKan {

	Texture::Texture(Device* device, const CommandPool* commandPool, const std::string& imageFilePath, Sampler* sampler)
		:wDevice(device), wCommandPool(commandPool)
	{
		wDevice = device;

		int texWidth, texHeight, texChannles;
		stbi_uc* pixels = stbi_load(imageFilePath.c_str(), &texWidth, &texHeight, &texChannles, STBI_rgb_alpha);

		if (!pixels) {
			throw std::runtime_error("Error: failed to read image data");
		}

		mImage = new VulKan::Image(
			wDevice, texWidth, texHeight,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		InitTexture(pixels, texWidth, texHeight);

		stbi_image_free(pixels);
	}

	Texture::Texture(Device* device, const CommandPool* commandPool, Sampler* sampler, void* imageData, int Width, int Height)
		:wDevice(device), wCommandPool(commandPool)
	{

		mImage = new VulKan::Image(
			wDevice, Width, Height,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		InitTexture(imageData, Width, Height);

		mImageInfo.imageLayout = mImage->getLayout();
		mImageInfo.imageView = mImage->getImageView();
		mImageInfo.sampler = sampler->getSampler();
	}

	void Texture::InitTexture(void* imageData, int Width, int Height) {
		int texSize = Width * Height * 4;

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
			wCommandPool
		);

		mImage->fillImageData(texSize, imageData);

		mImage->setImageLayout(
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			region,
			wCommandPool
		);
	}

	Texture::~Texture() {
		delete mImage;
	}
}