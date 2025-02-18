#pragma once
#include "../Vulkan/buffer.h"
#include "../Vulkan/device.h"
#include "../Vulkan/descriptorSet.h"
#include "../Vulkan/pipeline.h"
#include "../Vulkan/swapChain.h"
#include "../Vulkan/renderPass.h"
#include "PixelTexture.h"
#include "../GlobalStructural.h"

namespace VulKan {

	class VisualEffect
	{
	public:
		VisualEffect(VulKan::Device* device);
		~VisualEffect();

		//初始化描述符
		void initUniformManager(
			int frameCount, //GPU画布的数量
			const VkDescriptorSetLayout mDescriptorSetLayout,//渲染管线要的提交内容
			std::vector<VulKan::Buffer*> VPMstdBuffer,//玩家相机的变化矩阵 
			VulKan::Sampler* sampler//图片采样器
		);

		void RecordingCommandBuffer(VulKan::RenderPass* R, VulKan::SwapChain* S, VulKan::Pipeline* P) {
			wRenderPass = R;
			wSwapChain = S;
			wPipeline = P;
			initCommandBuffer();
		}

		void initCommandBuffer();

		inline void GetCommandBuffer(std::vector<VkCommandBuffer>* Vector, unsigned int F) {
			Vector->push_back(mCommandBuffer[F]->getCommandBuffer());
		}

		inline void SetPos(double x, double y, double z, int Index) {
			mUniform.mModelMatrix = glm::translate(glm::mat4(1.0), glm::vec3(x, y, z));
			mUniformParams[1]->mBuffers[Index]->updateBufferByMap(&mUniform, sizeof(ObjectUniform));
		}

		inline void SetPosAngle(double x, double y, double z, double anlge, int Index) {
			mUniform.mModelMatrix = glm::translate(glm::mat4(1.0), glm::vec3(x, y, z));
			mUniform.mModelMatrix = glm::rotate(mUniform.mModelMatrix, (float)glm::radians(anlge * 180.0 / 3.14), glm::vec3(0.0, 0.0, 1.0));
			mUniformParams[1]->mBuffers[Index]->updateBufferByMap(&mUniform, sizeof(ObjectUniform));
		}

	private://模型变换矩阵
		ObjectUniform mUniform{};

	private://模型   顶点，UV，顶点索引
		VulKan::Buffer* mPositionBuffer{ nullptr };
		VulKan::Buffer* mUVBuffer{ nullptr };
		VulKan::Buffer* mIndexBuffer{ nullptr };
		size_t mIndexDatasSize = 0;

		VulKan::PixelTexture* mPixelTexture{ nullptr }; //贴图

	private://描述模型   位置   贴图
		std::vector<VulKan::UniformParameter*> mUniformParams{};

		VulKan::DescriptorPool* mDescriptorPool{ nullptr };
		VulKan::DescriptorSet* mDescriptorSet{ nullptr };//指令录制用的数据
		VulKan::CommandPool* mCommandPool{ nullptr };
		VulKan::CommandBuffer** mCommandBuffer{ nullptr };

	private://储存
		VulKan::Device* wDevice{ nullptr };
		VulKan::Pipeline* wPipeline{ nullptr };
		VulKan::SwapChain* wSwapChain{ nullptr };
		VulKan::RenderPass* wRenderPass{ nullptr };
	};

}