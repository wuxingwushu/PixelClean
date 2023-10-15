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

		~Texture();

		[[nodiscard]] auto getImage() const { return mImage; }
		
		//[[nodiscard]] auto getSampler() const { return mSampler; }

		[[nodiscard]] VkDescriptorImageInfo& getImageInfo() { return mImageInfo; }

		[[nodiscard]] VkDescriptorImageInfo* getImageInfoP() { return &mImageInfo; }

	private:
		Device* mDevice{ nullptr };
		Image* mImage{ nullptr };
		VkDescriptorImageInfo mImageInfo{};
	};

}