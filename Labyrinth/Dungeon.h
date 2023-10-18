#pragma once
#include "../Vulkan/buffer.h"
#include "../Vulkan/descriptorSet.h"
#include "../Vulkan/commandBuffer.h"
#include "../Vulkan/renderPass.h"
#include "../Vulkan/swapChain.h"
#include "../Vulkan/pipeline.h"


#include "../Physics/SquarePhysics.h"

#include "../Tool/PerlinNoise.h"
#include "../BlockS/PixelS.h"
#include "../VulkanTool/Calculate.h"
#include "../GlobalVariable.h"
#include "../Tool/BlockData.h"
#include "../GlobalStructural.h"
#include "TextureLibrary.h"

#include "PathfindingDecorator.h"

namespace GAME {

	class PixelTexture;

	struct Dungeonmiwustruct {
		unsigned int size;
		int x;
		int y;
		unsigned int y_size;
		unsigned int x_size;
		float angel;
		int Color;
	};

	struct TextureAndBuffer {
		VulKan::Buffer* mBufferS = nullptr;
		VulKan::PixelTexture* mPixelTexture = nullptr;
		unsigned int Type = 0;
		unsigned int MistPointerX = 0;
		unsigned int MistPointerY = 0;
		short* PixelWallNumber = nullptr;//周围墙壁数量
		VulKan::DescriptorSet* mGIFDescriptorSet = nullptr;//GIF
	};

	class Dungeon;

	struct DungeonDestroyStruct {
		TextureAndBuffer* wTextureAndBuffer = nullptr;
		Dungeon* wDungeon = nullptr;
	};

	class Dungeon : public PathfindingDecorator
	{
	public:

		Dungeon(VulKan::Device* device, unsigned int X, unsigned int Y, SquarePhysics::SquarePhysics* squarePhysics, float GamePlayerX, float GamePlayerY);
		~Dungeon();

		//初始化描述符
		void initUniformManager(
			int frameCount, //GPU画布的数量
			const VkDescriptorSetLayout mDescriptorSetLayout,//渲染管线要的提交内容
			std::vector<VulKan::Buffer*> VPMstdBuffer,//玩家相机的变化矩阵 
			VulKan::Sampler* sampler,//图片采样器
			TextureLibrary* textureLibrary
		);

		//录制全部 CommandBuffer
		void initCommandBuffer();

		//初次录制全部 CommandBuffer
		void RecordingCommandBuffer(VulKan::RenderPass* R, VulKan::SwapChain* S, VulKan::Pipeline* P, VulKan::Pipeline* GifP) {
			wRenderPass = R;
			wSwapChain = S;
			wPipeline = P;
			wGIFPipeline = GifP;
			initCommandBuffer();
		}

		//获取地图 CommandBuffer
		void GetCommandBuffer(std::vector<VkCommandBuffer>* Vector, unsigned int F) {
			Vector->push_back(mCommandBuffer[F]->getCommandBuffer());
		}

		//获取迷雾 CommandBuffer
		void GetMistCommandBuffer(std::vector<VkCommandBuffer>* Vector, unsigned int F) {
			Vector->push_back(mMistCommandBuffer[F]->getCommandBuffer());
		}

		//根据输入的位置对区块位置进行更新
		MovePlateInfo UpPos(float x, float y) {
			return mMoveTerrain->UpDataPos(x, y);
		}

		//根据世界坐标获取对于的区块
		SquarePhysics::MoveTerrain<TextureAndBuffer>::RigidBodyAndModel* GetTerrain(float x, float y) {
			return mMoveTerrain->CalculateGetRigidBodyAndModel(x, y);
		}

		unsigned int wFrameCount{ 0 };//帧缓冲数

		//获取柏林噪声
		unsigned int GetNoise(double x, double y) {
			return mPerlinNoise->noise(x * 0.05f, y * 0.05f, 0.5) * TextureNumber;
		}

		//******************************* AI寻路地图 *******************************

		void UpdateAIPathfindingBlock(int x, int y);
		
		//获取对于墙壁数量指针
		short* GetPixelWallPointer(int x, int y) {
			return &(mMoveTerrain->GetRigidBodyAndModel(x / 16, y / 16)->mModel->PixelWallNumber[((x % 16) * 16) + (y % 16)]);
		}
		/*******************************************************/
		//获取对于墙壁数量
		virtual bool GetPixelWallNumber(int x, int y) {
			if ((x >= 0) && (x < (mNumberX * 16)) && (y >= 0) && (y < (mNumberY * 16))) {
				return mMoveTerrain->GetRigidBodyAndModel(x / 16, y / 16)->mModel->PixelWallNumber[((x % 16) * 16) + (y % 16)] <= 0;
			}
			else {
				return false;
			}
		}
		//射线检测
		virtual SquarePhysics::CollisionInfo RadialCollisionDetection(int x, int y, int Ex, int Ey) {
			return mMoveTerrain->RadialCollisionDetection({ x,y }, { Ex,Ey });
		};
		/*******************************************************/
		//计算点附近的墙壁数量
		void PixelWallNumberCalculate(short* PixelWallNumber, int x, int y) {
			int posX, posY;
			int Range1A = (x < 9 ? -x : -9), Range1B = ((x + 10) > (mNumberX * 16) ? ((mNumberX * 16) - x) : 10);
			int Range2A = (y < 9 ? -y : -9), Range2B = ((y + 10) > (mNumberY * 16) ? ((mNumberY * 16) - y) : 10);
			*PixelWallNumber = 0;
			for (int ix = Range1A; ix < Range1B; ++ix)
			{
				posX = x + ix;
				for (int iy = Range2A; iy < Range2B; ++iy)
				{
					posY = y + iy;
					if (mMoveTerrain->GetRigidBodyAndModel(posX / 16, posY / 16)->mGridDecorator->GetFixedCollisionBool({ posX % 16, posY % 16 })) {
						++(*PixelWallNumber);
					}
				}
			}
		}

		//删除像素时对周围的有墙壁的数-1
		void PixelWallNumberReduce(int x, int y) {
			int posX, posY;
			int Range1A = (x < 9 ? -x : -9), Range1B = ((x + 10) > (mNumberX * 16) ? ((mNumberX * 16) - x) : 10);
			int Range2A = (y < 9 ? -y : -9), Range2B = ((y + 10) > (mNumberY * 16) ? ((mNumberY * 16) - y) : 10);
			short* LData;
			for (int ix = Range1A; ix < Range1B; ++ix)
			{
				posX = x + ix;
				for (int iy = Range2A; iy < Range2B; ++iy)
				{
					posY = y + iy;
					LData = GetPixelWallPointer(posX, posY);
					if (*LData > 0) {
						--(*LData);
					}
				}
			}
		}

		//计算一个区块所有像素的墙壁数
		void BlockPixelWallNumber(int x, int y) {
			x = x * 16;
			y = y * 16;
			unsigned int posX, posY;
			bool B;
			for (size_t ix = 0; ix < 16; ++ix)
			{
				posX = x + ix;
				B = ((posX >= 8) || (posX <= ((mNumberX * 16) - 8)));
				for (size_t iy = 0; iy < 16; ++iy)
				{
					posY = y + iy;
					if (B && ((posY >= 8) || (posY <= ((mNumberY * 16) - 8)))) {
						PixelWallNumberCalculate(GetPixelWallPointer(posX, posY), posX, posY);
					}
					else {
						*GetPixelWallPointer(posX, posY) = 1;
					}
				}
			}
		}

	public://GIF
		// GIF CommandBuffer 状态
		enum GIFCommandBufferState
		{
			NotEnabled = 0,		//未启用
			Enabled,			//启用
			RequestUpdate,		//请求更新
		};

		//一个 GIF 区块 的控制器
		struct BlockCommandBuffer
		{
			std::mutex* mMutex = nullptr;
			VulKan::DescriptorPool* mGIFDescriptorPool{ nullptr };	//描述符池
			VulKan::CommandBuffer** mCommandBuffer = nullptr;
			GIFCommandBufferState State = NotEnabled;
		};

		//一个 GIF 的数据
		struct BlockGifData
		{
			VulKan::DescriptorSet* mDescriptorSet = nullptr;
			VulKan::Buffer* mBuffer = nullptr;
			VulKan::Buffer* mBufferUV = nullptr;

			bool operator<(const BlockGifData& other) const {
				return mDescriptorSet < other.mDescriptorSet;
			}

			bool operator==(const BlockGifData& other) const {
				return mDescriptorSet == other.mDescriptorSet;
			}
		};

		BlockData<BlockGifData, BlockCommandBuffer>* mBlockData = nullptr;//GIF的区块数据

		VulKan::Pipeline* wGIFPipeline = nullptr;	//储存的 GIF Pipeline
		TextureLibrary* wTextureLibrary = nullptr;	//储存的 GIF 库
		unsigned int mGIFBlockNumber = 0;						//GIF区块数量
		VulKan::CommandPool** mGIFCommandPool{ nullptr };		//指令池
		BlockCommandBuffer* mGIFBlockCommandBuffer{ nullptr };	//GIF区块 CommandBuffer

		//获取动图 CommandBuffer
		void GetGIFCommandBuffer(std::vector<VkCommandBuffer>* Vector, unsigned int F) {
			for (size_t i = 0; i < mBlockData->GetApplyNumber(); i++)
			{
				Vector->push_back(mBlockData->GetBlockDataS()[i]->mHandle->mCommandBuffer[F]->getCommandBuffer());
			}
		}

		//更新 GIF动画 到下一帧
		void UpDataGIF() {
			ObjectUniformGIF* mUniform;
			BlockData<BlockGifData, BlockCommandBuffer>::BlockDataT* Pointer;
			for (size_t i = 0; i < mBlockData->GetApplyNumber(); i++)
			{
				Pointer = mBlockData->GetBlockDataS()[i];
				for (size_t i2 = 0; i2 < Pointer->mNumber; i2++)
				{
					mUniform = (ObjectUniformGIF*)Pointer->DataS[i2].mBuffer->getupdateBufferByMap();
					++mUniform->zhen;
					if (mUniform->zhen > 24) {
						mUniform->zhen = 0;
					}
					Pointer->DataS[i2].mBuffer->endupdateBufferByMap();
				}
			}
		}

		//更新需要更新的 GIF区块 CommandBuffer
		void UpDataGIFCommandBuffer();
		void GIFCommandBuffer(BlockData<BlockGifData, BlockCommandBuffer>::BlockDataT* Pointer);

		//添加 GIF
		VulKan::DescriptorSet* AddGIF(VulKan::Buffer* buffer, VulKan::Texture* texture, VulKan::Buffer* bufferUV);

		//删除 GIF
		void popGIF(BlockGifData data);
	public://破坏
		const unsigned int mNumberX;
		const unsigned int mNumberY;

		int* LSPointer = nullptr;//贴图数据指针
		void Destroy(int x, int y, bool Bool);//贴图修改

		const unsigned int mSquareSideLength = 16;

		//计算可视范围
		void UpdataMist(int x, int y, float ang);

		bool Pointerkaiguan = true;
		unsigned char* LSMistPointer = nullptr;
		VulKan::PixelTexture* WarfareMist{ nullptr };			//每块的贴图
		VulKan::Buffer* WallBool{ nullptr };//储存碰撞(是否是墙壁)
		void UpdataMistData(int x, int y);
		int pianX = 0;
		int pianY = 0;


		std::vector<std::future<void>> MultithreadingGenerate;
		std::vector<VulKan::PixelTexture*>MultithreadingPixelTexture;
	private://迷雾
		//初始化战争迷雾
		void InitMist();
		//销毁战争迷雾
		void DeleteMist();

		//计算
		VulKan::Calculate* mCalculate = nullptr;//GPU计算
		Dungeonmiwustruct wymiwustruct{};
		VulKan::Buffer* jihsuanTP{ nullptr };//储存计算结果的缓存
		VulKan::Buffer* information{ nullptr };//储存原数据的缓存(图片纹理)
		VulKan::Buffer* CalculateIndex{ nullptr };//计算UV像素位置索引
		//渲染
		VulKan::Buffer*** mMistUVBuffer{ nullptr };		//模型UV缓存

		VulKan::DescriptorSet*** mMistDescriptorSet{ nullptr };//指令录制用的数据
		VulKan::CommandBuffer** mMistCommandBuffer{ nullptr };

	private:

		PerlinNoise* mPerlinNoise = nullptr;

		VulKan::Buffer* mPositionBuffer{ nullptr };	//模型点缓存
		VulKan::Buffer* mUVBuffer{ nullptr };		//模型UV缓存
		VulKan::Buffer* mIndexBuffer{ nullptr };	//模型顶点索引缓存
		size_t mIndexDatasSize{ 0 };				//顶点数量


		//获取  顶点和UV  VkBuffer数组
		[[nodiscard]] std::vector<VkBuffer> getVertexBuffers() const {
			return { mPositionBuffer->getBuffer(), mUVBuffer->getBuffer() };
		}

		VulKan::DescriptorPool* mDescriptorPool{ nullptr };//描述符池
		VulKan::DescriptorSet*** mDescriptorSet{ nullptr };//指令录制用的数据
		TextureAndBuffer** mTextureAndBuffer{ nullptr };//描述符Buffer 和 贴图

		VulKan::CommandPool* mCommandPool{ nullptr };		//指令池
		VulKan::CommandBuffer** mCommandBuffer{ nullptr };	//指令缓存
	public:
		SquarePhysics::MoveTerrain<TextureAndBuffer>* mMoveTerrain{ nullptr };//地图物理模型
	private:
		DungeonDestroyStruct** mDungeonDestroyStruct{ nullptr };
	private://储存数据
		VulKan::Device* wDevice{ nullptr };
		VulKan::RenderPass* wRenderPass{ nullptr };
		VulKan::Pipeline* wPipeline{ nullptr };
		VulKan::SwapChain* wSwapChain{ nullptr };
		SquarePhysics::SquarePhysics* wSquarePhysics{ nullptr };
		VkDescriptorSetLayout wDescriptorSetLayout;
		std::vector<VulKan::Buffer*> wVPMstdBuffer;//玩家相机的变化矩阵
	};

}