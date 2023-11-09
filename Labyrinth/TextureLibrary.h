#pragma once
#include "../VulkanTool/Texture.h"
#include "../Tool/ContinuousMap.h"

namespace GAME {

	class TextureLibrary
	{
	public:
		struct TextureToUVInfo
		{
			VulKan::Texture* mTexture = nullptr;
			VulKan::Buffer* mUV = nullptr;
			//图片各个方向切成多少
			unsigned int X = 1;
			unsigned int Y = 1;
		};

		TextureLibrary(VulKan::Device* device, VulKan::CommandPool* commandPool, VulKan::Sampler* sampler, std::string Path, bool UVbool = true);

		~TextureLibrary();

		//获取动图数据
		TextureToUVInfo GetTextureUV(std::string TextureName) {
			return *mTextureLibraryData->Get(TextureName);
		}

		ContinuousMap<std::string, TextureToUVInfo>* GetDataMap() {
			return mTextureLibraryData;
		}

	private:
		ContinuousMap<std::string, TextureToUVInfo>* mTextureLibraryData = nullptr;
	};

}