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
		Calculate(Device* Device, std::vector<CalculateStruct>* CalculateStructS, const char* Comp);
		~Calculate();

		[[nodiscard]] inline constexpr 
		CommandBuffer* GetCommandBuffer() noexcept {
			return mCommandBuffer; 
		}

		void begin();

		void end();
	private:
		Device* wDevice{ nullptr };
		CommandPool* mCommandPool{ nullptr };
		CommandBuffer* mCommandBuffer{ nullptr };


		VkDescriptorSetLayout mDescriptorSetLayout{ VK_NULL_HANDLE };
		VkDescriptorPool mDescriptorPool{ VK_NULL_HANDLE };
		VkDescriptorSet mDescriptorSet{ VK_NULL_HANDLE };

		VkPipeline mPipeline{ VK_NULL_HANDLE };
		VkPipelineLayout mPipelineLayout{ VK_NULL_HANDLE };
		VkShaderModule mShaderModule{ VK_NULL_HANDLE };
	};

}