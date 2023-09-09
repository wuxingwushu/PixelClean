#pragma once
#include "../Vulkan/buffer.h"
#include "../Vulkan/descriptorSet.h"
#include "../Vulkan/commandBuffer.h"
#include "../Vulkan/renderPass.h"
#include "../Vulkan/swapChain.h"
#include "../Vulkan/pipeline.h"


#include "../Physics/MoveTerrain.h"


namespace GAME {

	class PixelTexture;

	struct TextureAndBuffer {
		VulKan::Buffer** mBufferS = nullptr;
		PixelTexture* mPixelTexture = nullptr;
	};

	class Dungeon
	{
	public:

		Dungeon(VulKan::Device* device, unsigned int X, unsigned int Y);
		~Dungeon();

		//初始化描述符
		void initUniformManager(
			int frameCount, //GPU画布的数量
			const VkDescriptorSetLayout mDescriptorSetLayout,//渲染管线要的提交内容
			std::vector<VulKan::Buffer*> VPMstdBuffer,//玩家相机的变化矩阵 
			VulKan::Sampler* sampler//图片采样器
		);

		void initCommandBuffer();

		void RecordingCommandBuffer(VulKan::RenderPass* R, VulKan::SwapChain* S, VulKan::Pipeline* P) {
			mRenderPass = R;
			mSwapChain = S;
			mPipeline = P;
			initCommandBuffer();
		}

		void GetCommandBuffer(std::vector<VkCommandBuffer>* Vector, unsigned int F) {
			Vector->push_back(mCommandBuffer[F]->getCommandBuffer());
		}

		void UpPos(float x, float y) {
			mMoveTerrain->UpDataPos(x, y);
		}


		unsigned int wFrameCount{ 0 };
	private:
		const unsigned int mNumberX;
		const unsigned int mNumberY;

		VulKan::Buffer* mPositionBuffer{ nullptr };
		VulKan::Buffer* mUVBuffer{ nullptr };
		VulKan::Buffer* mIndexBuffer{ nullptr };
		size_t mIndexDatasSize{ 0 };

		//获取  顶点和UV  VkBuffer数组
		[[nodiscard]] std::vector<VkBuffer> getVertexBuffers() const {
			return { mPositionBuffer->getBuffer(), mUVBuffer->getBuffer() };
		}

		VulKan::DescriptorPool* mDescriptorPool{ nullptr };
		VulKan::DescriptorSet*** mDescriptorSet{ nullptr };//指令录制用的数据
		TextureAndBuffer** mTextureAndBuffer{ nullptr };

		VulKan::CommandPool*mCommandPool{ nullptr };
		VulKan::CommandBuffer** mCommandBuffer{ nullptr };

		SquarePhysics::MoveTerrain<TextureAndBuffer>* mMoveTerrain{ nullptr };
	private://储存
		VulKan::Device* wDevice{ nullptr };
		VulKan::RenderPass* mRenderPass{ nullptr };
		VulKan::Pipeline* mPipeline{ nullptr };
		VulKan::SwapChain* mSwapChain{ nullptr };
	};

}