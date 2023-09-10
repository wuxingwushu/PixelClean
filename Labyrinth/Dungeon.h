#pragma once
#include "../Vulkan/buffer.h"
#include "../Vulkan/descriptorSet.h"
#include "../Vulkan/commandBuffer.h"
#include "../Vulkan/renderPass.h"
#include "../Vulkan/swapChain.h"
#include "../Vulkan/pipeline.h"


#include "../Physics/SquarePhysics.h"

#include "../Tool/PerlinNoise.h"


namespace GAME {

	class PixelTexture;

	struct TextureAndBuffer {
		VulKan::Buffer** mBufferS = nullptr;
		PixelTexture* mPixelTexture = nullptr;
		unsigned int Type = 0;

		~TextureAndBuffer() {
			std::cout << "Delete ? " << std::endl;
		}
	};

	class Dungeon
	{
	public:

		Dungeon(VulKan::Device* device, unsigned int X, unsigned int Y, SquarePhysics::SquarePhysics* squarePhysics);
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

		void UpPos(float x, float y) {
			mMoveTerrain->UpDataPos(x, y);
		}

		SquarePhysics::MoveTerrain<TextureAndBuffer>::RigidBodyAndModel* GetTerrain(float x, float y){
			return mMoveTerrain->CalculateGetRigidBodyAndModel(x, y);
		}

		unsigned int wFrameCount{ 0 };//帧缓冲数

	public://破坏
		int* LSPointer = nullptr;//贴图数据指针
		void Destroy(int x, int y, bool Bool);//贴图修改

		const unsigned int mSquareSideLength = 16;
		PerlinNoise* mPerlinNoise = nullptr;
	private:
		const unsigned int mNumberX;
		const unsigned int mNumberY;

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
	private://储存数据
		VulKan::Device* wDevice{ nullptr };
		VulKan::RenderPass* wRenderPass{ nullptr };
		VulKan::Pipeline* wPipeline{ nullptr };
		VulKan::SwapChain* wSwapChain{ nullptr };
		SquarePhysics::SquarePhysics* mSquarePhysics{ nullptr };
	};

}