#pragma once
#include "../../base.h"
#include "../../Vulkan/pipeline.h"
#include "../../Vulkan/swapChain.h"
#include "../../Tool/MemoryPool.h"
#include "../../Vulkan/descriptorSet.h"
#include "../../Vulkan/description.h"
#include "../../Vulkan/buffer.h"
#include "../../VulKanTool/PixelTexture.h"


#include "../../GeneralCalculationGPU/GPU.h"

#include "../../Physics/SquarePhysics.h"

#include "../../Tool/PerlinNoise.h"


#include "../../GlobalVariable.h"

#include "../../Tool/Queue.h"

#include "../../GlobalStructural.h"

#include "../../VulkanTool/Calculate.h"

#include "../PathfindingDecorator.h"

#include "../../Tool/GridNavigation.h"

struct miwustruct_2 {
	unsigned int size;
	int x;
	int y;
	unsigned int y_size;
	float angel;
	int Color;
};


namespace GAME {
	class FixedMaze : public PathfindingDecorator
	{
	public:
		FixedMaze(SquarePhysics::SquarePhysics* squarePhysics);
		~FixedMaze();

		//重新生成迷宫
		void AgainGenerateLabyrinth(int X, int Y);
		//初始化迷宫
		void InitLabyrinth(VulKan::Device* device, int X, int Y);
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
		inline void GetCommandBuffer(std::vector<VkCommandBuffer>* Vector, unsigned int F) {
			Vector->push_back(mThreadCommandBufferS[F]->getCommandBuffer());
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

		//多线程分配指令缓存
		void ThreadUpdateCommandBuffer();
		//录制缓存指令
		void ThreadCommandBufferToUpdate(unsigned int FrameCount, unsigned int BufferCount, unsigned int AddresShead, unsigned int Count);		

		//中心
		int mOriginX = 0;
		int mOriginY = 0;
		int numberX = 0;//Block 横排多少个
		int numberY = 0;//Block 纵排多少个
		//像素点是否是墙壁
		int* BlockPixelS = nullptr;


		GridNavigation<short>* mGridNavigation = nullptr;
		/*******************************************************/
		//获取点附近的墙壁数量
		virtual inline bool GetPixelWallNumber(unsigned int x, unsigned int y) {
			x += mOriginX;
			y += mOriginY;
			if ((x < (numberX)) && (y < (numberY))) {
				return mGridNavigation->Get(x,y) >= 9;
			}
			else {
				return false;
			}
		}
		//射线检测
		virtual SquarePhysics::CollisionInfo RadialCollisionDetection(int x, int y, int Ex, int Ey) {
			return mFixedSizeTerrain->RadialCollisionDetection({ x,y }, { Ex,Ey });
		};
		/*******************************************************/


		bool RangeLegitimate(int x, int y);
		glm::ivec2 GetLegitimateGeneratePos();

	private:

		VulKan::DescriptorPool* mDescriptorPool{ nullptr };//描述符池
		std::vector<VulKan::UniformParameter*>* mUniformParams = nullptr;
		
		VulKan::PixelTexture* PixelTextureS{ nullptr };//每块的贴图
		VulKan::DescriptorSet* mDescriptorSet{ nullptr };//位置 贴图 的数据

		

		VulKan::Buffer* mPositionBuffer{ nullptr };//点坐标
		VulKan::Buffer* mUVBuffer{ nullptr };//点对应的UV
		VulKan::Buffer* mIndexBuffer{ nullptr };//点排序
		size_t mIndexDatasSize = 0;//点的数量

		unsigned int mFrameCount = 0;
		VulKan::CommandPool** mThreadCommandPoolS = nullptr;
		VulKan::CommandBuffer** mThreadCommandBufferS = nullptr;//地图
		
	private://储存信息
		VulKan::Device* mDevice = nullptr;
		VulKan::RenderPass* mRenderPass = nullptr;
		VkDescriptorSetLayout mDescriptorSetLayout{VK_NULL_HANDLE};
		VulKan::Pipeline* mPipeline = nullptr;
		VulKan::SwapChain* mSwapChain = nullptr;
		VulKan::Sampler* mSampler = nullptr;
		std::vector<VulKan::Buffer*>* mVPMstdBuffer = nullptr;

	public://物理
		SquarePhysics::SquarePhysics* wSquarePhysics = nullptr;
		SquarePhysics::FixedSizeTerrain* mFixedSizeTerrain = nullptr;
		Queue<PixelState>* mPixelQueue = nullptr;
	};
}