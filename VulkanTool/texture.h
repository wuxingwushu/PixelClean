#pragma once

#include "../base.h"
#include "../VulKan/image.h"//����ͼƬ
#include "../VulKan/sampler.h"//����ͼƬ������
#include "../VulKan/device.h"
#include "../VulKan/commandPool.h"



namespace VulKan {

	class Texture {
	public:
		Texture(Device* device, const CommandPool* commandPool, const std::string &imageFilePath, Sampler* sampler);

		Texture(Device* device, const CommandPool* commandPool, Sampler* sampler, void* imageData, int Width, int Height);

		void InitTexture(void* imageData, int Width, int Height);

		~Texture();

		[[nodiscard]] auto getImage() const { return mImage; }
		
		//[[nodiscard]] auto getSampler() const { return mSampler; }

		[[nodiscard]] VkDescriptorImageInfo& getImageInfo() { return mImageInfo; }

		[[nodiscard]] VkDescriptorImageInfo* getImageInfoP() { return &mImageInfo; }

	private:
		Device* wDevice{ nullptr };
		Image* mImage{ nullptr };
		VkDescriptorImageInfo mImageInfo{};
		const CommandPool* wCommandPool{ nullptr };
	};

}