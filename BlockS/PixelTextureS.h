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

		[[namespace]] PixelTexture* getPixelTexture(unsigned int PixelTextureNumber) const { 
			return mPixelTextureS[PixelTextureNumber]; 
		}

	private:
		PixelTexture** mPixelTextureS;
	};
}