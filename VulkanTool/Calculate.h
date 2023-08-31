#pragma once
#include "../Vulkan/commandPool.h"
#include "../Vulkan/commandBuffer.h"

namespace VulKan {

	struct CalculateStruct
	{
		VkDescriptorBufferInfo* mBufferInfo{ VK_NULL_HANDLE };
	};

	class Calculate
	{
	public:
		Calculate(VkDevice Device, std::vector<CalculateStruct>* CalculateStructS, const char* Comp);
		~Calculate();

		CommandBuffer* GetCommandBuffer() { return mCommandBuffer; }
	private:
		VkDevice wDevice;
		CommandPool* mCommandPool = nullptr;
		CommandBuffer* mCommandBuffer = nullptr;


		VkDescriptorSetLayout mDescriptorSetLayout{ VK_NULL_HANDLE };
		VkDescriptorPool mDescriptorPool{ VK_NULL_HANDLE };
		VkDescriptorSet mDescriptorSet{ VK_NULL_HANDLE };

		VkPipeline mPipeline{ VK_NULL_HANDLE };
		VkPipelineLayout mPipelineLayout{ VK_NULL_HANDLE };
		VkShaderModule mShaderModule{ VK_NULL_HANDLE };
	};

}