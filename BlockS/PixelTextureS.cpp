#include "PixelTextureS.h"
#include "../DebugLog.h"

namespace GAME {
	PixelTextureS::PixelTextureS(
		VulKan::Device* device,
		const VulKan::CommandPool* commandPool,
		VulKan::Sampler* sampler
	) {
		LOGD("PixelTextureS::PixelTextureS() called");
		mPixelTextureS = new VulKan::PixelTexture* [TextureNumber];

		for (int i = 0; i < TextureNumber; i++)
		{
			mPixelTextureS[i] = new VulKan::PixelTexture(device, commandPool, pixelS[i], 16, 16, 4, sampler);
		}
	}

	PixelTextureS::~PixelTextureS() {
		LOGD("PixelTextureS::~PixelTextureS() called");
		for (int i = 0; i < TextureNumber; i++)
		{
			delete mPixelTextureS[i];
		}
		delete[] mPixelTextureS;
	}
}