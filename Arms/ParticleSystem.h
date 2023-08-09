#pragma once
#include "../base.h"
#include "../Vulkan/pipeline.h"
#include "../Vulkan/swapChain.h"
#include "../Tool/MemoryPool.h"
#include "../Vulkan/descriptorSet.h"
#include "../Vulkan/description.h"
#include "../Vulkan/buffer.h"
#include "../texture/PixelTexture.h"
#include "../Tool/PileUp.h"


namespace GAME {

	struct Particle {
		PixelTexture* Pixel;
		std::vector<VulKan::Buffer*>* Buffer;
	};

	class ParticleSystem
	{
	public:
		ParticleSystem(VulKan::Device* device, int Number);

		//初始化描述符
		void initUniformManager(
			VulKan::Device* device, //设备
			const VulKan::CommandPool* commandPool, //指令池
			int frameCount, //GPU画布的数量
			const VkDescriptorSetLayout mDescriptorSetLayout,//渲染管线要的提交内容
			std::vector<VulKan::Buffer*> VPMstdBuffer,//玩家相机的变化矩阵 
			VulKan::Sampler* sampler//图片采样器
		);

		//获取  顶点和UV  VkBuffer数组
		[[nodiscard]] std::vector<VkBuffer> getVertexBuffers() const {
			return { mPositionBuffer->getBuffer(), mUVBuffer->getBuffer() };
		}

		//获得 索引Buffer
		[[nodiscard]] VulKan::Buffer* getIndexBuffer() const { return mIndexBuffer; }

		//获得 索引数量
		[[nodiscard]] size_t getIndexCount() const { return mIndexDatasSize; }

		void RecordingCommandBuffer(VulKan::RenderPass* R, VulKan::SwapChain* S, VulKan::Pipeline* P) {
			mRenderPass = R;
			mSwapChain = S;
			mPipeline = P;
			ThreadUpdateCommandBuffer();
		}

		void GetCommandBuffer(std::vector<VkCommandBuffer>* Vector, unsigned int F) {
			for (size_t i = 0; i < (ThreadS / 3); i++)
			{
				Vector->push_back(mThreadCommandBufferS[(F * (ThreadS / 3)) + i]->getCommandBuffer());
			}
		};


		void ThreadUpdateCommandBuffer();
		void ThreadCommandBufferToUpdate(unsigned int FrameCount, unsigned int BufferCount, unsigned int AddresShead, unsigned int Count);

		~ParticleSystem();

		int mNumber;//Particle 多少个
		PileUp<Particle>* mParticle = nullptr;//每个粒子
	private:
		VulKan::DescriptorPool* mDescriptorPool{ nullptr };//描述符池
		std::vector<VulKan::UniformParameter*>* mUniformParams{ nullptr };
		VulKan::DescriptorSet** mDescriptorSet{ nullptr };//位置 贴图 的数据
		PixelTexture** PixelTextureS{ nullptr };//每块的贴图

		VulKan::Buffer* mPositionBuffer{ nullptr };//点坐标
		VulKan::Buffer* mUVBuffer{ nullptr };//点对应的UV
		VulKan::Buffer* mIndexBuffer{ nullptr };//点排序
		size_t mIndexDatasSize;//点的数量

		unsigned int ThreadS;
		VulKan::CommandPool** mThreadCommandPoolS;
		VulKan::CommandBuffer** mThreadCommandBufferS;

		VulKan::RenderPass* mRenderPass;
		VulKan::Pipeline* mPipeline;
		VulKan::SwapChain* mSwapChain;
	};
}