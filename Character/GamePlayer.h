#pragma once
#include "../base.h"
#include "../Vulkan/buffer.h"
#include "../Vulkan/device.h"
#include "../Vulkan/descriptorSet.h"
#include "../Vulkan/description.h"
#include "../Vulkan/pipeline.h"
#include "../Vulkan/swapChain.h"
#include "../Vulkan/renderPass.h"

#include "../Physics/SquarePhysics.h"

namespace GAME {
	class GamePlayer
	{
	public:
		GamePlayer(VulKan::Device* device, VulKan::Pipeline* pipeline, VulKan::SwapChain* swapChain, VulKan::RenderPass* renderPass, float X, float Y);
		~GamePlayer();

		//初始化描述符
		void initUniformManager(
			VulKan::Device* device, //设备
			const VulKan::CommandPool* commandPool, //指令池
			int frameCount, //GPU画布的数量
			PixelTexture* texturepath, //贴图
			const VkDescriptorSetLayout mDescriptorSetLayout,//渲染管线要的提交内容
			std::vector<VulKan::Buffer*> VPMstdBuffer,//玩家相机的变化矩阵 
			VulKan::Sampler* sampler//图片采样器
		);

		void InitCommandBuffer();

		void SetKey(unsigned int key) {
			Key = key;
		}


		//更新描述符，模型位置
		//mode = false 模式：持续更新位置（运动） [ 变换矩阵，那个GPU画布 ];
		//mode = true  模式：一次设置位置（静止） [ 变换矩阵，GPU画布数量，true ];
		void setGamePlayerMatrix(glm::mat4 Matrix, const int& frameCount, bool mode = false);

		//获取  顶点和UV  VkBuffer数组
		[[nodiscard]] std::vector<VkBuffer> getVertexBuffers() const {
			return { mPositionBuffer->getBuffer(), mUVBuffer->getBuffer() };
		}

		//获得 索引Buffer
		[[nodiscard]] VulKan::Buffer* getIndexBuffer() const { return mIndexBuffer; }

		//获得 索引数量
		[[nodiscard]] size_t getIndexCount() const { return mIndexDatasSize; }

		//获得 描述符
		[[nodiscard]] VkDescriptorSet getDescriptorSet(int frameCount) const { return mDescriptorSet->getDescriptorSet(frameCount); }

		[[nodiscard]] VkCommandBuffer getCommandBuffer(int i)  const { return mBufferCopyCommandBuffer[i]->getCommandBuffer(); }

		[[nodiscard]] unsigned int GetKey() { return Key; }

	private://模型变换矩阵
		unsigned int Key;
		ObjectUniform mUniform;

	private://模型   顶点，UV，顶点索引
		VulKan::Buffer* mPositionBuffer{ nullptr };
		VulKan::Buffer* mUVBuffer{ nullptr };
		VulKan::Buffer* mIndexBuffer{ nullptr };
		size_t mIndexDatasSize;

	private://描述模型   位置   贴图
		std::vector<VulKan::UniformParameter*> mUniformParams;

		VulKan::DescriptorPool* mDescriptorPool{ nullptr };
		VulKan::DescriptorSet* mDescriptorSet{ nullptr };//指令录制用的数据

	private:
		VulKan::CommandPool* mBufferCopyCommandPool;
		VulKan::CommandBuffer** mBufferCopyCommandBuffer;

		VulKan::Pipeline* mPipeline;
		VulKan::SwapChain* mSwapChain;
		VulKan::RenderPass* mRenderPass;

	public://物理
		SquarePhysics::ObjectCollision* mObjectCollision = nullptr;
	};
}
