#pragma once
#include "../base.h"
#include "Block.h"
#include "PixelTextureS.h"
#include "../Vulkan/pipeline.h"
#include "../Vulkan/swapChain.h"
#include "../Tool/MemoryPool.h"


namespace GAME {

	class BlockS {
	public:

		BlockS
		(
			VulKan::Device* device,
			VulKan::CommandPool* commandPool,
			int frameCount,
			const VkDescriptorSetLayout descriptorSetLayout,
			std::vector<VulKan::Buffer*> VPMBufferS,
			glm::vec3 Camerapos, 
			int width,
			int height,
			VulKan::Sampler* sampler,
			VkRenderPass pass,
			VulKan::Pipeline* pipeline,
			VulKan::SwapChain* swapChain
		);

		~BlockS();

		//摄像机移动事件
		void CameraMoveEvent(float CameraX, float CameraY);

		//录制重新录制指令
		void CommandBufferToUpdate();





		//屏幕大小改变，指令重新录制
		//屏幕大小改变，mDescriptorSetLayout 也要更新，因为渲染管线更新了
		void WindowResizedToUpdate(
			VulKan::Pipeline* pipeline,
			VulKan::SwapChain* swapChain,
			VkRenderPass Pass
		);




		[[nodiscard]] Block* getBlock(int x, int y) const { return mBlock[x][y]; }

		[[nodiscard]] VkCommandBuffer getDoubleCommandBuffer(int i,int j) const {
			if (mDoubleCommandBufferBool) {
				return mThreadCommandBufferS[(i * ThreadCommandBufferNumber) + j]->getCommandBuffer();
			}
			else {
				return mThreadCommandBufferS[(i * ThreadCommandBufferNumber) + j + (ThreadCommandBufferNumber * mFrameCount)]->getCommandBuffer();
			}
		}

	



	private://储存常用的信息
		VulKan::Device* mDevice;
		VulKan::CommandPool* mCommandPool;
		int mFrameCount;
		VkDescriptorSetLayout mDescriptorSetLayout;
		std::vector<VulKan::Buffer*> mVPMBufferS;
		VulKan::Sampler* mSampler;
		VkRenderPass mRenderPass;
		VulKan::Pipeline* mPipeline;
		VulKan::SwapChain* mSwapChain;


	
	public:
		int numberX;//Block 横排多少个
		int numberY;//Block 纵排多少个
	private:
		static const int mPixel = 16;//像素数 16 * 16
		Block*** mBlock{ nullptr };

	public:
		//相机移动，区块更新
		int mCameraBlockX;//相机 X 上一帧的 int(/Block) 后的数据
		int mCameraBlockY;//相机 Y 上一帧的 int(/Block) 后的数据
	private:

		// Block 生成
		unsigned int BlockCount = 0;//多少个要更新的Block
		//储存 生成那个位置
		int BlockToUpdateX[10000];
		int BlockToUpdateY[10000];
		//储存 那个Block
		int BlockS_X[10000];
		int BlockS_Y[10000];

		void LongitudinalToUpdate(int Y);//纵向更新
		void TransverseToUpdate(int X);//横向更新


		// Block 待销毁池
		static const int DestroyBlockPoolSize = 1000;
		static const int DestroyNumber = 4;//池子的数量
		int DestroyBlock_i = 0;//当前使用第几个池子
		Block*** DestroyBlockPool;//池子
		unsigned int* DestroyBlockNumber;//池子里销毁对象数量
		
		


	private://多线程更新

		bool mGenerate = true;// 是否 生成完成，CommandBuffer 录制完成，是否继续检测生成地图



		bool mThreadGenerateBool = false;//多线程生成 是否开启

		
		//unsigned int ThreadBlockS;//多线程更新Block数量
		VulKan::CommandPool* mBufferCopyCommandPool;
		VulKan::CommandBuffer* mBufferCopyCommandBuffer;//生成时提交 Buffer Copy Buffer
		bool* mThreadGenerateBoolS;//多线程的各个线程是否生成完毕
		void ThreadDistribution();//对获得的生成信息继续生成
		void ThreadToUpdateBlock(unsigned int AddresShead, unsigned int Count, unsigned int CommandBufferCount);//多线程生成函数



		bool mBufferCopyBool = false;//生成完成，向GPU提交生成信息，提交完毕就开始多线程录制


		// 多 CommandBuffer，  
		// mFrameCount * 2 个 CommandBuffer，
		// 一个显示，一个更新，更新好了，再和显示的替换，这样就不会卡顿
		VulKan::CommandPool** mThreadCommandPoolS;
		VulKan::CommandBuffer** mThreadCommandBufferS;
		bool mDoubleCommandBufferBool = true;// 双CommandBuffer 交替使用的控制量，
	public:
		int ThreadCommandBufferNumber;
	private:
		bool* mThreadBool;// ThreadCommandBuffer 录制的 信号量 都为 true 时，线程都完成录制
		void ThreadUpdateCommandBuffer();
		void ThreadCommandBufferToUpdate(unsigned int FrameCount, unsigned int BufferCount, unsigned int AddresShead, unsigned int Count);//多线程录制函数

		
	public:
		MemoryPool<Block, 4096000>** mMemoryPoolBlock;
		// 贴图群
		PixelTextureS* mPixelTextureS;
	};

}