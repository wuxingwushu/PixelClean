#include "ShaderTexture.h"


namespace VulKan {



	ShaderTexture::ShaderTexture(
		Device* device, 
		const CommandPool* commandPool, 
		unsigned int texWidth, 
		unsigned int texHeight, 
		unsigned int ChannelsNumber, 
		Sampler* sampler)
		:wDevice(device)
	{
		mPixelTexture = new PixelTexture(device, commandPool, nullptr, texWidth, texHeight, ChannelsNumber, sampler);

		mParameter = new VulKan::Buffer(device, sizeof(ShaderTextureParameter),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | 
			VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE
		);
		ShaderTextureParameter* ShaderTextureParameterP = (ShaderTextureParameter*)mParameter->getupdateBufferByMap();
		ShaderTextureParameterP->time = 0.0f;
		ShaderTextureParameterP->Width = texWidth;
		ShaderTextureParameterP->Height = texHeight;
		mParameter->endupdateBufferByMap();

		mBackground = new VulKan::Buffer(device, (texWidth * texHeight * ChannelsNumber),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE
		);

		std::vector<CalculateStruct> CalculateStructInfo{};
		CalculateStructInfo.push_back({ &mParameter->getBufferInfo() });
		CalculateStructInfo.push_back({ &mBackground->getBufferInfo() });
		mCalculate = new Calculate(device, &CalculateStructInfo, GridBackground_spv);
	}

	ShaderTexture::~ShaderTexture()
	{
		delete mPixelTexture;
		delete mParameter;
		delete mBackground;
		delete mCalculate;
	}

	void ShaderTexture::CalculationScreen(float time) {
		ShaderTextureParameter* ShaderTextureParameterP = (ShaderTextureParameter*)mParameter->getupdateBufferByMap();
		ShaderTextureParameterP->time += time;
		mParameter->endupdateBufferByMap();

		mCalculate->begin();
		vkCmdDispatch(mCalculate->GetCommandBuffer()->getCommandBuffer(),
			(uint32_t)ceil((mPixelTexture->getImage()->getWidth() * mPixelTexture->getImage()->getHeight()) / float(64)),
			1,
			1);
		mPixelTexture->RewritableDataType(mCalculate->GetCommandBuffer());
		mCalculate->GetCommandBuffer()->copyBufferToImage(//将计算的迷雾结果更新到图片上
			mBackground->getBuffer(),
			mPixelTexture->getImage()->getImage(),
			mPixelTexture->getImage()->getLayout(),
			mPixelTexture->getImage()->getWidth(),
			mPixelTexture->getImage()->getHeight()
		);
		mPixelTexture->RewriteDataTypeOptimization(mCalculate->GetCommandBuffer());
		mCalculate->end();
		mCalculate->GetCommandBuffer()->submitSync(wDevice->getGraphicQueue(), VK_NULL_HANDLE);
	}

}