#pragma once
#include "../Vulkan/buffer.h"
#include "../Vulkan/descriptorSet.h"
#include "../Vulkan/pipeline.h"
#include "../Vulkan/swapChain.h"
#include "../Physics/SquarePhysics.h"


namespace GAME {

	struct UVDynamicDiagramStruct
	{
		glm::mat4 mModelMatrix;
		unsigned int UnitSize = 0;		//单元大小
		unsigned int CurrentFrame = 0;	//当前帧
	};

	class UVDynamicDiagram
	{
	public:
		UVDynamicDiagram(VulKan::Device* device, VulKan::Pipeline* pipeline, VulKan::SwapChain* swapChain, VulKan::RenderPass* renderPass, std::vector<VulKan::Buffer*> VPMstdBuffer, SquarePhysics::SquarePhysics* SquarePhysics);
		~UVDynamicDiagram();

		void InitCommandBuffer();

		//获取 CommandBuffer
		inline void GetCommandBuffer(std::vector<VkCommandBuffer>* Vector, unsigned int F) {
			Vector->push_back(mCommandBuffer[F]->getCommandBuffer());
		}

		//设置位置
		void SetPosition(float x, float y, float z) {
			//mUVDynamicDiagramStruct.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
			UVDynamicDiagramStruct* P = (UVDynamicDiagramStruct*)mPositionToData->getupdateBufferByMap();
			P->mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
			mPositionToData->endupdateBufferByMap();
		}

		//动画事件
		void AnimationEvent(float time) {
			AccumulatedTime += time;
			UVDynamicDiagramStruct* P = (UVDynamicDiagramStruct*)mPositionToData->getupdateBufferByMap();
			P->mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(mIndexAnimationGrid->GetPosX(), mIndexAnimationGrid->GetPosY(), 0));
			P->mModelMatrix = glm::rotate(P->mModelMatrix, (float)glm::radians(mIndexAnimationGrid->GetAngleFloat() * 180.0 / 3.14159265359), glm::vec3(0.0, 0.0, 1.0));
			if (AccumulatedTime > 2.5f) {
				AccumulatedTime = 0;
				
				++P->CurrentFrame;
				if (P->CurrentFrame >= FrameNumber) {
					P->CurrentFrame = 0;
				}
				mIndexAnimationGrid->SetCurrentFrame(P->CurrentFrame);
				mUVDynamicDiagramStruct.CurrentFrame = P->CurrentFrame;
				
			}
			mPositionToData->endupdateBufferByMap();
		}

		int GetPixelIndex(int x, int y) {
			return uvdh[9 * mUVDynamicDiagramStruct.CurrentFrame + (x * 3 + y)];
		}

		SquarePhysics::IndexAnimationGrid* mIndexAnimationGrid{ nullptr };

	private:
		UVDynamicDiagramStruct mUVDynamicDiagramStruct{};
		VulKan::Buffer* mPositionToData{ nullptr };//位置和数据
		VulKan::Buffer* mBasisChart{ nullptr };//基础原图
		VulKan::Buffer* mAnimationUVS{ nullptr };//动画UV，（以最长的UV数据为单元）
		VulKan::Buffer* mPixelPosition{ nullptr };//每个像素位置
		VulKan::Buffer* mSequentialIndex{ nullptr };//配合 VertexBuffer 使用的 0  1  2  3 ... 顺序的数字(单元大小 数量)
		unsigned int Quantity = 9;//点的数量
		unsigned int FrameNumber = 8;//帧数
		float AccumulatedTime = 0;
	private:
		VulKan::DescriptorPool* mDescriptorPool{ nullptr };
		VulKan::DescriptorSet* mDescriptorSet{ nullptr };
		VulKan::CommandPool* mCommandPool{ nullptr };
		VulKan::CommandBuffer** mCommandBuffer{ nullptr };

	private://储存
		VulKan::Pipeline* wPipeline{ nullptr };
		VulKan::SwapChain* wSwapChain{ nullptr };
		VulKan::RenderPass* wRenderPass{ nullptr };
		SquarePhysics::SquarePhysics* wSquarePhysics{ nullptr };


		int uvdh[72] = {
			-1, -1, -1,
			 0,  1,  2,
			-1, -1, -1,

			 0, -1, -1,
			-1,  1, -1,
			-1, -1,  2,

			-1,  0, -1,
			-1,  1, -1,
			-1,  2, -1,

			-1, -1,  0,
			-1,  1, -1,
			 2, -1, -1,

			-1, -1, -1,
			 2,  1,  0,
			-1, -1, -1,

			 2, -1, -1,
			-1,  1, -1,
			-1, -1,  0,

			-1,  2, -1,
			-1,  1, -1,
			-1,  0, -1,

			-1, -1,  2,
			-1,  1, -1,
			 0, -1, -1,
		};
	};

}