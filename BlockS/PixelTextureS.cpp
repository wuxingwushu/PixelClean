#include "PixelTextureS.h"

namespace GAME {
	PixelTextureS::PixelTextureS(
		VulKan::Device* device,
		const VulKan::CommandPool* commandPool,
		VulKan::Sampler* sampler
	) {
		mPixelTextureS = new VulKan::PixelTexture* [TextureNumber];

		for (int i = 0; i < TextureNumber; i++)
		{
			mPixelTextureS[i] = new VulKan::PixelTexture(device, commandPool, pixelS[i], 16, 16, 4, sampler);
		}
	}

	PixelTextureS::~PixelTextureS() {
		for (int i = 0; i < TextureNumber; i++)
		{
			delete mPixelTextureS[i];
		}
	}
}