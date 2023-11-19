#pragma once
#include "../base.h"
#include "../VulKan/image.h"
#include "../VulKan/sampler.h"
#include "../VulKan/device.h"
#include "../VulKan/commandPool.h"


namespace VulKan {

	class Texture {
	public:
		Texture(Device* device, const CommandPool* commandPool, const std::string &imageFilePath, Sampler* sampler);

		Texture(Device* device, const CommandPool* commandPool, Sampler* sampler, void* imageData, int Width, int Height);

		void InitTexture(void* imageData, int Width, int Height);

		~Texture();

		[[nodiscard]] inline constexpr auto getImage() const noexcept { return mImage; }
		
		//[[nodiscard]] auto getSampler() const { return mSampler; }

		[[nodiscard]] inline constexpr VkDescriptorImageInfo& getImageInfo() noexcept { return mImageInfo; }

		[[nodiscard]] inline constexpr VkDescriptorImageInfo* getImageInfoP() noexcept { return &mImageInfo; }

	private:
		Device* wDevice{ nullptr };
		Image* mImage{ nullptr };
		VkDescriptorImageInfo mImageInfo{};
		const CommandPool* wCommandPool{ nullptr };
	};

}