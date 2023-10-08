#pragma once
#include "../base.h"
#include "../Vulkan/buffer.h"
#include "../Vulkan/device.h"
#include "../Vulkan/descriptorSet.h"
#include "../Vulkan/description.h"

namespace GAME {
	
	class Block {
	public:

		Block(VulKan::Device* device, int X, int Y, int MemoryPooli, bool collision);
		int MemoryPool_i;
		bool Collision;

		~Block();

		//初始化描述符
		void initUniformManager(
			VulKan::Device* device, //设备
			const VulKan::CommandPool* commandPool, //指令池
			int frameCount, //GPU画布的数量
			VulKan::PixelTexture* texturepath, //贴图
			const VkDescriptorSetLayout mDescriptorSetLayout,//渲染管线要的提交内容
			std::vector<VulKan::Buffer*> VPMstdBuffer,//玩家相机的变化矩阵 
			VulKan::Sampler* sampler//图片采样器
		);

		void getBlockCommandBuffer(VulKan::CommandBuffer* BlockCommandBuffer);

		//更新描述符，模型位置
		//mode = false 模式：持续更新位置（运动） [ 变换矩阵，那个GPU画布 ];
		//mode = true  模式：一次设置位置（静止） [ 变换矩阵，GPU画布数量，true ];
		void setBlockMatrix(glm::mat4 Matrix, const int& frameCount, bool mode = false);

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

		int mX;
		int mY;

	private://模型变换矩阵
		ObjectUniform mUniform;

	private://模型   顶点，UV，顶点索引

		//这种普通数组，比vector动态数组速度快一倍，所以大数据就用普通数据好点
		/*
		为什么 是 16.001f 是因为 GPU 在进行点的矩阵变换时，float 会产生计算偏差，导致某一位置的 Block 出现空隙，略大点可以填充空隙。
		*/
		float mPositions[12] = {
			-8.0f, -8.0f, 0.0f,
			8.001f, -8.0f, 0.0f,
			8.001f, 8.001f, 0.0f,
			-8.0f, 8.001f, 0.0f,
		};



		std::vector<float> mUVs{};
		std::vector<unsigned int> mIndexDatas{};

		VulKan::Buffer* mPositionBuffer{ nullptr };
		VulKan::Buffer* mUVBuffer{ nullptr };
		VulKan::Buffer* mIndexBuffer{ nullptr };
		size_t mIndexDatasSize;


	private://描述模型   位置   贴图
		std::vector<VulKan::UniformParameter*> mUniformParams;

		VulKan::DescriptorPool* mDescriptorPool{ nullptr };
		VulKan::DescriptorSet* mDescriptorSet{ nullptr };//指令录制用的数据
	};

}