#pragma once
#include "../base.h"
#include "../VulKan/image.h"
#include "../VulKan/sampler.h"
#include "../VulKan/device.h"
#include "../Vulkan/buffer.h"
#include "PixelTexture.h"
#include "Calculate.h"

namespace VulKan {

	struct ShaderTextureParameter {
		float time;
		unsigned int Width;
		unsigned int Height;
	};
	
	class ShaderTexture
	{
	public:
		ShaderTexture(
			Device* device,
			const CommandPool* commandPool,
			unsigned int texWidth,
			unsigned int texHeight,
			unsigned int ChannelsNumber,
			Sampler* sampler
		);
		~ShaderTexture();


		[[nodiscard]] inline auto getImage() const noexcept { return mPixelTexture->getImage(); }

		[[nodiscard]] inline VkDescriptorImageInfo& getImageInfo() noexcept { return mPixelTexture->getImageInfo(); }

		[[nodiscard]] inline constexpr auto getTexture() const noexcept { return mPixelTexture; }

		void CalculationScreen(float time);

	private:
		PixelTexture* mPixelTexture{ nullptr };
		Calculate* mCalculate{ nullptr };

		//buffer
		Buffer* mParameter{ nullptr };
		Buffer* mBackground{ nullptr };


		Device* wDevice{ nullptr };
		
	};

}