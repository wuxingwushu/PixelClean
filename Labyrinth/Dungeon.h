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
	};

	class Dungeon;

	struct DungeonDestroyStruct {
		TextureAndBuffer* wTextureAndBuffer = nullptr;
		Dungeon* wDungeon = nullptr;
	};

	class Dungeon
	{
	public:

		Dungeon(VulKan::Device* device, unsigned int X, unsigned int Y, SquarePhysics::SquarePhysics* squarePhysics, float GamePlayerX, float GamePlayerY);
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
			wRenderPass = R;
			wSwapChain = S;
			wPipeline = P;
			initCommandBuffer();
		}

		void GetCommandBuffer(std::vector<VkCommandBuffer>* Vector, unsigned int F) {
			Vector->push_back(mCommandBuffer[F]->getCommandBuffer());
		}

		void GetMistCommandBuffer(std::vector<VkCommandBuffer>* Vector, unsigned int F) {
			Vector->push_back(mMistCommandBuffer[F]->getCommandBuffer());
		}

		MovePlateInfo UpPos(float x, float y) {
			return mMoveTerrain->UpDataPos(x, y);
		}

		SquarePhysics::MoveTerrain<TextureAndBuffer>::RigidBodyAndModel* GetTerrain(float x, float y){
			return mMoveTerrain->CalculateGetRigidBodyAndModel(x, y);
		}

		unsigned int wFrameCount{ 0 };//帧缓冲数

		unsigned int GetNoise(double x, double y) {
			return mPerlinNoise->noise(x * 0.05f, y * 0.05f, 0.5) * TextureNumber;
		}

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
		void UpdataMistData();
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

		VulKan::CommandPool*mCommandPool{ nullptr };		//指令池
		VulKan::CommandBuffer** mCommandBuffer{ nullptr };	//指令缓存

		SquarePhysics::MoveTerrain<TextureAndBuffer>* mMoveTerrain{ nullptr };//地图物理模型

		DungeonDestroyStruct** mDungeonDestroyStruct{ nullptr };
	private://储存数据
		VulKan::Device* wDevice{ nullptr };
		VulKan::RenderPass* wRenderPass{ nullptr };
		VulKan::Pipeline* wPipeline{ nullptr };
		VulKan::SwapChain* wSwapChain{ nullptr };
		SquarePhysics::SquarePhysics* wSquarePhysics{ nullptr };
	};

}