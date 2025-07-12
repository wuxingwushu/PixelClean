#pragma once
#include "../VulKanTool/PixelTexture.h"
#include "PixelS.h"

namespace GAME {
	class PixelTextureS {
	public:
		PixelTextureS(
			VulKan::Device* device,
			const VulKan::CommandPool* commandPool,
			VulKan::Sampler* sampler
		);

		~PixelTextureS();

		inline VulKan::PixelTexture* getPixelTexture(unsigned int PixelTextureNumber) {
			return mPixelTextureS[PixelTextureNumber]; 
		}

	private:
		VulKan::PixelTexture** mPixelTextureS;
	};
}