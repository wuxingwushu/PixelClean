#pragma once
#include "../Vulkan/descriptorSet.h"
#include "../Vulkan/buffer.h"
#include "../VulkanTool/Texture.h"
#include "../Tool/PileUp.h"
#include "../Tool/Queue.h"
#include "../GlobalStructural.h"
#include "../Vulkan/renderPass.h"
#include "../Vulkan/pipeline.h"
#include "../Vulkan/swapChain.h"

namespace GAME {

	class DamagePrompt
	{
	public:
		DamagePrompt(VulKan::Device* device, 
			const VkDescriptorSetLayout mDescriptorSetLayout, std::vector<VulKan::Buffer*> camera, 
			VulKan::Texture* texture, unsigned int frameCount, unsigned int size);

		~DamagePrompt();

		void RecordingCommandBuffer(VulKan::RenderPass* R, VulKan::SwapChain* S, VulKan::Pipeline* P) {
			wRenderPass = R;
			wSwapChain = S;
			wPipeline = P;
			initCommandBuffer();
		}

		void initCommandBuffer();

		//获取某帧的 CommandBuffer
		void GetCommandBuffer(std::vector<VkCommandBuffer>* Vector, unsigned int F) {
			Vector->push_back(mCommandBuffer[F]->getCommandBuffer());
		}

		void AddDamagePrompt(float Angle);

		void UpDataDamagePrompt(float x, float y, float time);

	private://模型   顶点，UV，顶点索引
		VulKan::Buffer* mPositionBuffer{ nullptr };
		VulKan::Buffer* mUVBuffer{ nullptr };
		VulKan::Buffer* mIndexBuffer{ nullptr };
		size_t mIndexDatasSize = 0;

	private:
		unsigned int mSize = 0;
		Queue<float>* mDamagePromptAngle{ nullptr };

		PileUp<VulKan::Buffer*>* mFreeLocation{ nullptr };//空闲可用
		Queue<VulKan::Buffer*>* mPlaceOfUse{ nullptr };//使用中

		VulKan::DescriptorPool* mDescriptorPool{ nullptr };
		VulKan::DescriptorSet** mDescriptorSet{ nullptr };

		VulKan::CommandPool* mCommandPool{ nullptr };
		VulKan::CommandBuffer** mCommandBuffer{ nullptr };

	private:
		VulKan::RenderPass* wRenderPass{ nullptr };
		VulKan::SwapChain* wSwapChain{ nullptr };
		VulKan::Pipeline* wPipeline{ nullptr };
	};

}