#include "ImGuiTexture.h"

namespace GAME {
	
	ImGuiTexture::ImGuiTexture(
		VulKan::Device* device, 
		VulKan::SwapChain* swapChain, 
		VulKan::CommandPool* commandPool, 
		VulKan::Sampler* sampler, 
		unsigned int FreeDescriptorSize)
		:wDevice(device), wSwapChain(swapChain)
	{
		mTextureLibrary = new TextureLibrary(device, commandPool, sampler, "./Resource/ImGuiImage/", false);
		mDescriptorSetMap = new ContinuousMap<std::string, VulKan::DescriptorSet*>(mTextureLibrary->GetDataMap()->GetNumber() + FreeDescriptorSize + 1);

		std::vector<VulKan::UniformParameter*> mUniformParameter;
		VulKan::UniformParameter* textureParam = new VulKan::UniformParameter();
		textureParam->mBinding = 0;
		textureParam->mCount = 1;
		textureParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		textureParam->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		mUniformParameter.push_back(textureParam);

		mDescriptorPool = new VulKan::DescriptorPool(device);
		mDescriptorPool->build(mUniformParameter, swapChain->getImageCount(), mTextureLibrary->GetDataMap()->GetNumber() + FreeDescriptorSize);

		VkDescriptorSetLayoutBinding binding[1] = {};
		binding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding[0].descriptorCount = 1;
		binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		VkDescriptorSetLayoutCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.bindingCount = 1;
		info.pBindings = binding;
		vkCreateDescriptorSetLayout(device->getDevice(), &info, nullptr, &mVkDescriptorSetLayout);

		for (size_t i = 0; i < mTextureLibrary->GetDataMap()->GetNumber(); i++)
		{
			textureParam->mTexture = mTextureLibrary->GetTextureUV(mTextureLibrary->GetDataMap()->GetIndexKey(i)).mTexture;
			VulKan::DescriptorSet** mDescriptorSet = mDescriptorSetMap->New(mTextureLibrary->GetDataMap()->GetIndexKey(i));
			(*mDescriptorSet) = new VulKan::DescriptorSet(device, mUniformParameter,
				mVkDescriptorSetLayout, mDescriptorPool, swapChain->getImageCount());
		}
		delete textureParam;
	}

	ImGuiTexture::~ImGuiTexture()
	{
		delete mTextureLibrary;
		for (auto i : *mDescriptorSetMap) {
			delete i;
		}
		delete mDescriptorSetMap;
		delete mDescriptorPool;
	}

	void ImGuiTexture::AddTexture(std::string name, VulKan::PixelTexture* Texture) {
		std::vector<VulKan::UniformParameter*> mUniformParameter;
		VulKan::UniformParameter* textureParam = new VulKan::UniformParameter();
		textureParam->mBinding = 0;
		textureParam->mCount = 1;
		textureParam->mDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		textureParam->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		mUniformParameter.push_back(textureParam);

		textureParam->mPixelTexture = Texture;
		VulKan::DescriptorSet** mDescriptorSet = mDescriptorSetMap->New(name);
		(*mDescriptorSet) = new VulKan::DescriptorSet(wDevice, mUniformParameter,
			mVkDescriptorSetLayout, mDescriptorPool, wSwapChain->getImageCount());

		delete textureParam;
	}

}