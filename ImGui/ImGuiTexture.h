#pragma once
#include "../Vulkan/descriptorSet.h"
#include "../Labyrinth/TextureLibrary.h"
#include "../Vulkan/pipeline.h"
#include "../Vulkan/sampler.h"
#include "../Vulkan/swapChain.h"

namespace GAME {

	class ImGuiTexture
	{
	public:
		ImGuiTexture(VulKan::Device* device, VulKan::SwapChain* swapChain, VulKan::CommandPool* commandPool, VulKan::Sampler* sampler);
		~ImGuiTexture();

		VulKan::DescriptorSet* GetTexture(std::string name) {
			return *mDescriptorSetMap->Get(name);
		}

	private:
		VulKan::DescriptorPool* mDescriptorPool{ nullptr };
		TextureLibrary* mTextureLibrary{ nullptr };
		VkDescriptorSetLayout mVkDescriptorSetLayout;
		ContinuousMap<std::string, VulKan::DescriptorSet*>* mDescriptorSetMap{ nullptr };
	};

}