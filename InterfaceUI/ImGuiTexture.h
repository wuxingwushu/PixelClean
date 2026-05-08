#pragma once
#include "../Vulkan/descriptorSet.h"
#include "../VulkanTool/TextureLibrary.h"
#include "../Vulkan/pipeline.h"
#include "../Vulkan/sampler.h"
#include "../Vulkan/swapChain.h"
#include "../VulkanTool/PixelTexture.h"

namespace GAME {

	class ImGuiTexture
	{
	public:
		ImGuiTexture(VulKan::Device* device, VulKan::SwapChain* swapChain, VulKan::CommandPool* commandPool, VulKan::Sampler* sampler, unsigned int FreeDescriptorSize = 0);
		~ImGuiTexture();

		VulKan::DescriptorSet* GetTexture(std::string name) {
			return *mDescriptorSetMap->Get(name);
		}

		void AddTexture(std::string name, VulKan::PixelTexture* Texture);

	private:
		VulKan::Device* wDevice{ nullptr };
		VulKan::SwapChain* wSwapChain{ nullptr };
		VulKan::DescriptorPool* mDescriptorPool{ nullptr };
		TextureLibrary* mTextureLibrary{ nullptr };
		VkDescriptorSetLayout mVkDescriptorSetLayout;
		ContinuousMap<std::string, VulKan::DescriptorSet*>* mDescriptorSetMap{ nullptr };
	};

}