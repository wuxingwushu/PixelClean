#pragma once
#include "../base.h"
#include "../Vulkan/pipeline.h"
#include "../Vulkan/swapChain.h"
#include "../Tool/MemoryPool.h"
#include "../Vulkan/descriptorSet.h"
#include "../Vulkan/description.h"
#include "../Vulkan/buffer.h"
#include "../texture/PixelTexture.h"

#include "GenerateMaze.h"


#include "../GeneralCalculationGPU/GPU.h"

#include "../Physics/SquarePhysics.h"

#include "../Tool/PerlinNoise.h"


#include "../GlobalVariable.h"

struct miwustruct {
	unsigned int size;
	int x;
	int y;
	unsigned int y_size;
	float angel;
	int Color;
};


namespace GAME {
	class Labyrinth
	{
	public:
		Labyrinth();

		void AgainGenerateLabyrinth(int X, int Y);

		void LoadLabyrinth(int X, int Y, int* PixelData, unsigned int* BlockTypeData);

		void InitLabyrinth(VulKan::Device* device, int X, int Y, bool** LlblockS = nullptr);

		void LabyrinthBuffer();

		//初始化描述符
		void initUniformManager(
			VulKan::Device* device, //设备
			int frameCount, //GPU画布的数量
			const VkDescriptorSetLayout DescriptorSetLayout,//渲染管线要的提交内容
			std::vector<VulKan::Buffer*>* VPMstdBuffer,//玩家相机的变化矩阵 
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

		//获取地图
		void GetCommandBuffer(std::vector<VkCommandBuffer>* Vector, unsigned int F) {
			Vector->push_back(mThreadCommandBufferS[F]->getCommandBuffer());
		};

		//获取迷雾
		void GetMistCommandBuffer(std::vector<VkCommandBuffer>* Vector, unsigned int F) {
			Vector->push_back(mMistCommandBufferS[F]->getCommandBuffer());
		};

		//void penzhang(unsigned int x, unsigned int y);
		void SetPixel(unsigned int x, unsigned int y, unsigned int Dx, unsigned int Dy);
		void SetPixel(unsigned int x, unsigned int y);
		void AddPixel(unsigned int x, unsigned int y);
		bool GetPixel(unsigned int x, unsigned int y);
		void UpDateMaps();

		//录制缓存指令
		void ThreadUpdateCommandBuffer();
		void ThreadCommandBufferToUpdate(unsigned int FrameCount, unsigned int BufferCount, unsigned int AddresShead, unsigned int Count);

		//void MIWU(int wjx, int wjy, float ang);

		//初始化战争迷雾
		void InitMist();
		//计算可视范围
		void UpdataMist(int wjx, int wjy, float ang);
		//销毁战争迷雾
		void DeleteMist();
		
		~Labyrinth();

		int numberX;//Block 横排多少个
		int numberY;//Block 纵排多少个
		bool** BlockS = nullptr;//是否是墙壁 16 * 16 
		int* BlockPixelS = nullptr;//像素点是否是墙壁
		unsigned int** BlockTypeS = nullptr;//地面类型
	private:
		bool UpDateMapsSwitch = false;//地图是否修改

		VulKan::DescriptorPool* mDescriptorPool{ nullptr };//描述符池
		std::vector<VulKan::UniformParameter*>* mUniformParams;
		
		PixelTexture* PixelTextureS{ nullptr };//每块的贴图
		VulKan::DescriptorSet* mDescriptorSet{ nullptr };//位置 贴图 的数据

		
		
		

		VulKan::Buffer* mPositionBuffer{ nullptr };//点坐标
		VulKan::Buffer* mUVBuffer{ nullptr };//点对应的UV
		VulKan::Buffer* mIndexBuffer{ nullptr };//点排序
		size_t mIndexDatasSize;//点的数量

		unsigned int mFrameCount;
		VulKan::CommandPool** mThreadCommandPoolS = nullptr;
		VulKan::CommandBuffer** mThreadCommandBufferS = nullptr;//地图
		VulKan::CommandBuffer** mMistCommandBufferS = nullptr;//迷雾



		
	private://储存信息
		VulKan::Device* mDevice = nullptr;
		VulKan::RenderPass* mRenderPass = nullptr;
		VkDescriptorSetLayout mDescriptorSetLayout;
		VulKan::Pipeline* mPipeline = nullptr;
		VulKan::SwapChain* mSwapChain = nullptr;
		VulKan::Sampler* mSampler = nullptr;
		std::vector<VulKan::Buffer*>* mVPMstdBuffer;




	private://战争迷雾
		PixelTexture* WarfareMist{ nullptr };//每块的贴图
		VulKan::DescriptorSet* mMistDescriptorSet{ nullptr };//位置 角度  射线颜色 的数据

		miwustruct wymiwustruct{};
		VulKan::Buffer* jihsuanTP{ nullptr };//储存计算结果的缓存
		VulKan::Buffer* information{ nullptr };//储存原数据的缓存(图片纹理)
		VulKan::Buffer* WallBool{ nullptr };//储存碰撞(是否是墙壁)

		VkDescriptorSetLayout descriptorSetLayout;
		VkDescriptorPool descriptorPool;
		VkDescriptorSet descriptorSet;

		VkPipeline pipeline;
		VkPipelineLayout pipelineLayout;
		VkShaderModule computeShaderModule;

		VulKan::CommandPool* mMistCalculateCommandPoolS = nullptr;
		VulKan::CommandBuffer* mMistCalculate = nullptr;

	public://物理
		SquarePhysics::FixedSizeTerrain* mFixedSizeTerrain = nullptr;
	};
}