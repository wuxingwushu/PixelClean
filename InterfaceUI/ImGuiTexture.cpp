#include "ImGuiTexture.h"
#include "../DebugLog.h"
#include <memory>  // std::unique_ptr

namespace GAME {

	ImGuiTexture::ImGuiTexture(
		VulKan::Device* device,
		VulKan::SwapChain* swapChain,
		VulKan::CommandPool* commandPool,
		VulKan::Sampler* sampler,
		unsigned int FreeDescriptorSize)
		:wDevice(device), wSwapChain(swapChain)
	{
		LOGD("ImGuiTexture::ImGuiTexture() called");
		mTextureLibrary = new TextureLibrary(device, commandPool, sampler, "./Resource/ImGuiImage/", false);
		mDescriptorSetMap = new ContinuousMap<std::string, VulKan::DescriptorSet*>(mTextureLibrary->GetDataMap()->GetNumber() + FreeDescriptorSize + 1);

		// 用 unique_ptr 管理 textureParam，防止 DescriptorSet 构造抛异常时泄漏
		auto textureParam = std::make_unique<VulKan::UniformParameter>();
		textureParam->mBinding = 0;
		textureParam->mCount = 1;
		textureParam->mDescriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		textureParam->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;

		std::vector<VulKan::UniformParameter*> mUniformParameter;
		mUniformParameter.push_back(textureParam.get());

		mDescriptorPool = new VulKan::DescriptorPool(device);
		mDescriptorPool->build(mUniformParameter, swapChain->getImageCount(), mTextureLibrary->GetDataMap()->GetNumber() + FreeDescriptorSize);

		VkDescriptorSetLayoutBinding binding[1] = {};
		binding[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
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
		// textureParam 由 unique_ptr 自动释放，无需手动 delete
	}

	ImGuiTexture::~ImGuiTexture()
	{
		LOGD("ImGuiTexture::~ImGuiTexture() called");
		delete mTextureLibrary;
		for (auto i : *mDescriptorSetMap) {
			delete i;
		}
		delete mDescriptorSetMap;
		delete mDescriptorPool;
	}

	void ImGuiTexture::AddTexture(std::string name, VulKan::PixelTexture* Texture) {
		// 用 unique_ptr 管理 textureParam，防止 DescriptorSet 构造抛异常时泄漏
		auto textureParam = std::make_unique<VulKan::UniformParameter>();
		textureParam->mBinding = 0;
		textureParam->mCount = 1;
		textureParam->mDescriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		textureParam->mStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		textureParam->mPixelTexture = Texture;

		std::vector<VulKan::UniformParameter*> mUniformParameter;
		mUniformParameter.push_back(textureParam.get());

		VulKan::DescriptorSet** mDescriptorSet = mDescriptorSetMap->New(name);
		(*mDescriptorSet) = new VulKan::DescriptorSet(wDevice, mUniformParameter,
			mVkDescriptorSetLayout, mDescriptorPool, wSwapChain->getImageCount());

		// textureParam 由 unique_ptr 自动释放，无需手动 delete
	}

}
