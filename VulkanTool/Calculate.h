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

		/*
		 * ===================== 独立模式（原有 API，向后兼容） =====================
		 *
		 * 适用于单个 Kernel 独自完成所有计算，不需要与其他 Kernel 共享 CommandBuffer 的场景。
		 * 用法：
		 *   calc->begin();                          // 录制新 CommandBuffer + 绑定 Pipeline + DescriptorSet
		 *   vkCmdDispatch(cmd, groups, 1, 1);       // 手动调用 dispatch
		 *   calc->end();                            // 结束录制
		 *   calc->GetCommandBuffer()->submitSync(queue);  // 提交
		 */

		[[nodiscard]] inline constexpr 
		CommandBuffer* GetCommandBuffer() noexcept {
			return mCommandBuffer; 
		}

		void begin();

		void end();

		/*
		 * ===================== 共享模式（新增 API） =====================
		 *
		 * 适用于多个 Calculate 实例共享同一个外部 CommandBuffer 的场景（如：物理引擎需要串行执行
		 * Arbiter → Joint → Junction 三个 Kernel，中间插入 VkMemoryBarrier）。
		 *
		 * 用法：
		 *   // 1. 创建一个共享 CommandBuffer
		 *   VulKan::CommandBuffer sharedCmd(device, commandPool);
		 *   sharedCmd.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		 *
		 *   for (iter) {
		 *       mCalcArbiter->recordDispatch(sharedCmd.getCommandBuffer(), groupsA);
		 *       vkCmdPipelineBarrier(sharedCmd.getCommandBuffer(), ...);  // 手动插入 Barrier
		 *
		 *       mCalcJoint->recordDispatch(sharedCmd.getCommandBuffer(), groupsJ);
		 *       vkCmdPipelineBarrier(sharedCmd.getCommandBuffer(), ...);
		 *
		 *       mCalcJunction->recordDispatch(sharedCmd.getCommandBuffer(), groupsJu);
		 *       vkCmdPipelineBarrier(sharedCmd.getCommandBuffer(), ...);
		 *   }
		 *
		 *   sharedCmd.end();
		 *   sharedCmd.submitSync(queue);
		 *
		 * 注意：
		 * - recordDispatch 不会调用 begin/end，调用者自行管理 CommandBuffer 生命周期
		 * - 需要在外部 CommandBuffer 处于 recording 状态时调用
		 * - 多个 Calculate 可以写入同一个 CommandBuffer，实现单次 submit 多 Kernel dispatch
		 */

		// 获取 Pipeline 句柄（用于外部绑定到自定义 CommandBuffer）
		[[nodiscard]] inline VkPipeline getPipeline() const noexcept { return mPipeline; }

		// 获取 PipelineLayout 句柄（用于外部绑定 DescriptorSet 或 push constant）
		[[nodiscard]] inline VkPipelineLayout getPipelineLayout() const noexcept { return mPipelineLayout; }

		// 获取 DescriptorSet 句柄（用于外部绑定到此 Calculate 的 SSBO 资源）
		[[nodiscard]] inline VkDescriptorSet getDescriptorSet() const noexcept { return mDescriptorSet; }

		// 向外部 CommandBuffer 录制：绑定此 Calculate 的 Pipeline + DescriptorSet + vkCmdDispatch
		// 调用者需确保 cmdBuffer 处于 recording 状态，且之后会手动调用 end/submit
		void recordDispatch(
			VkCommandBuffer cmdBuffer,
			uint32_t groupCountX,
			uint32_t groupCountY = 1,
			uint32_t groupCountZ = 1
		);

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