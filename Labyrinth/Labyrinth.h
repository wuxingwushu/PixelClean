#pragma once
#include "../base.h"
#include "../Vulkan/pipeline.h"
#include "../Vulkan/swapChain.h"
#include "../Tool/MemoryPool.h"
#include "../Vulkan/descriptorSet.h"
#include "../Vulkan/description.h"
#include "../Vulkan/buffer.h"
#include "../VulKanTool/PixelTexture.h"

#include "GenerateMaze.h"


#include "../GeneralCalculationGPU/GPU.h"

#include "../Physics/SquarePhysics.h"

#include "../Tool/PerlinNoise.h"


#include "../GlobalVariable.h"

#include "../Tool/Queue.h"

#include "../GlobalStructural.h"

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
		~Labyrinth();

		//重新生成迷宫
		void AgainGenerateLabyrinth(int X, int Y);
		//加载迷宫
		void LoadLabyrinth(int X, int Y, int* PixelData, unsigned int* BlockTypeData);
		//初始化迷宫
		void InitLabyrinth(VulKan::Device* device, int X, int Y, bool** LlblockS = nullptr);
		//迷宫缓存
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
		


		//更新迷宫破损情况
		void UpDateMaps();

		//获取破坏指针
		void GetPixelSPointer();
		//修改像素
		void SetPixelS(unsigned int x, unsigned int y, bool Switch);
		//结束破坏指针
		void EndPixelSPointer();

		unsigned char* TexturePointer = nullptr;
		unsigned char* TextureMist = nullptr;
		int* Mistwall = nullptr;

		//多线程分配指令缓存
		void ThreadUpdateCommandBuffer();
		//录制缓存指令
		void ThreadCommandBufferToUpdate(unsigned int FrameCount, unsigned int BufferCount, unsigned int AddresShead, unsigned int Count);

		//初始化战争迷雾
		void InitMist();
		//计算可视范围
		void UpdataMist(int wjx, int wjy, float ang);
		//销毁战争迷雾
		void DeleteMist();
		

		//中心
		int mOriginX = 0;
		int mOriginY = 0;
		int numberX = 0;//Block 横排多少个
		int numberY = 0;//Block 纵排多少个
		//是否是墙壁 16 * 16 
		bool** BlockS = nullptr;
		//像素点是否是墙壁
		int* BlockPixelS = nullptr;
		//地面类型
		unsigned int** BlockTypeS = nullptr;

		short** PixelWallNumber = nullptr;// 附近 17 * 17 的墙壁的数量
		//获取点附近的墙壁数量
		short GetPixelWallNumber(int x, int y);
		//计算点附近的墙壁数量
		void PixelWallNumberCalculate(int x, int y);
		//将 17 * 17 范围的点都加 1
		void PixelWallNumberAdd(int x, int y);
		//将 17 * 17 范围的点都减 1
		void PixelWallNumberReduce(int x, int y);
		//获取点是否是墙壁
		bool GetPixel(int x, int y);
		//获取位置是否合法
		bool GetPixelLegitimate(int x, int y);

		bool RangeLegitimate(int x, int y);
		glm::ivec2 GetLegitimateGeneratePos();

	private:

		VulKan::DescriptorPool* mDescriptorPool{ nullptr };//描述符池
		std::vector<VulKan::UniformParameter*>* mUniformParams;
		
		PixelTexture* PixelTextureS{ nullptr };//每块的贴图
		VulKan::DescriptorSet* mDescriptorSet{ nullptr };//位置 贴图 的数据

		

		VulKan::Buffer* mPositionBuffer{ nullptr };//点坐标
		VulKan::Buffer* mUVBuffer{ nullptr };//点对应的UV
		VulKan::Buffer* mIndexBuffer{ nullptr };//点排序
		size_t mIndexDatasSize;//点的数量

		unsigned int mFrameCount = 0;
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
		std::vector<VulKan::Buffer*>* mVPMstdBuffer = nullptr;




	private://战争迷雾
		PixelTexture* WarfareMist{ nullptr };//每块的贴图
		VulKan::DescriptorSet* mMistDescriptorSet{ nullptr };//位置 角度  射线颜色 的数据

		miwustruct wymiwustruct{};
		VulKan::Buffer* jihsuanTP{ nullptr };//储存计算结果的缓存
		VulKan::Buffer* information{ nullptr };//储存原数据的缓存(图片纹理)
		VulKan::Buffer* WallBool{ nullptr };//储存碰撞(是否是墙壁)

		VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
		VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };

		VkPipeline pipeline{ VK_NULL_HANDLE };
		VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
		VkShaderModule computeShaderModule{ VK_NULL_HANDLE };

		VulKan::CommandPool* mMistCalculateCommandPoolS = nullptr;
		VulKan::CommandBuffer* mMistCalculate = nullptr;

	public://物理
		SquarePhysics::FixedSizeTerrain* mFixedSizeTerrain = nullptr;
		Queue<PixelState>* mPixelQueue = nullptr;
	};
}