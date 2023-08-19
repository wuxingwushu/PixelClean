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
#include "../Tool/Queue.h"
#include "../GlobalStructural.h"

namespace GAME {
	class GamePlayer
	{
	public:
		GamePlayer(VulKan::Device* device, VulKan::Pipeline* pipeline, VulKan::SwapChain* swapChain, VulKan::RenderPass* renderPass, 
			SquarePhysics::SquarePhysics* SquarePhysics, float X, float Y);
		~GamePlayer();

		//初始化描述符
		void initUniformManager(
			VulKan::Device* device, //设备
			const VulKan::CommandPool* commandPool, //指令池
			int frameCount, //GPU画布的数量
			int textureID, //贴图ID
			const VkDescriptorSetLayout mDescriptorSetLayout,//渲染管线要的提交内容
			std::vector<VulKan::Buffer*> VPMstdBuffer,//玩家相机的变化矩阵 
			VulKan::Sampler* sampler//图片采样器
		);

		void InitCommandBuffer();

		void SetKey(unsigned int key) {
			Key = key;
		}

		//破坏指针
		unsigned char* TexturePointer = nullptr;
		//获取破坏指针
		void GetPixelSPointer();
		//设置像素
		void SetPixelS(unsigned int x, unsigned int y, bool Switch);
		//结束破坏指针
		void EndPixelSPointer();
		//更新玩家损伤情况
		void UpData();


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

		//获取玩家破坏情况数据
		[[nodiscard]] bool* GetBrokenData(){ return mBrokenData; }
		//玩家破坏情况数据，更新玩家破损
		void UpDataBroken(bool* Broken);

		//判断是否伤及玩家要害
		bool GetCrucial(int x, int y);
		//获取玩家是否阵亡
		[[nodiscard]] bool GetDeathInBattle(){ return DeathInBattle; }

	private://模型变换矩阵
		bool DeathInBattle = false;
		unsigned int Key = 0;
		ObjectUniform mUniform{};
		bool* mBrokenData = nullptr; //破坏情况数据

	private://模型   顶点，UV，顶点索引
		VulKan::Buffer* mPositionBuffer{ nullptr };
		VulKan::Buffer* mUVBuffer{ nullptr };
		VulKan::Buffer* mIndexBuffer{ nullptr };
		size_t mIndexDatasSize = 0;

		PixelTexture* mPixelTexture = nullptr; //贴图

	private://描述模型   位置   贴图
		std::vector<VulKan::UniformParameter*> mUniformParams{};

		VulKan::DescriptorPool* mDescriptorPool{ nullptr };
		VulKan::DescriptorSet* mDescriptorSet{ nullptr };//指令录制用的数据

	private:
		VulKan::CommandPool* mBufferCopyCommandPool = nullptr;
		VulKan::CommandBuffer** mBufferCopyCommandBuffer = nullptr;

		VulKan::Pipeline* mPipeline = nullptr;
		VulKan::SwapChain* mSwapChain = nullptr;
		VulKan::RenderPass* mRenderPass = nullptr;

		SquarePhysics::SquarePhysics* mSquarePhysics = nullptr;//物理
	public://物理
		SquarePhysics::ObjectCollision* mObjectCollision = nullptr;
		Queue<PixelState>* mPixelQueue = nullptr;
	};
}
