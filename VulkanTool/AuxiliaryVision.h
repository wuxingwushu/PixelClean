#pragma once
#include "../Vulkan/buffer.h"
#include "../Vulkan/device.h"
#include "../Vulkan/descriptorSet.h"
#include "PipelineS.h"
#include "../Vulkan/swapChain.h"
#include "../Vulkan/renderPass.h"
#include "PixelTexture.h"
#include "../Tool/ContinuousMap.h"

namespace VulKan {

	struct AuxiliaryLine
	{
		glm::vec3 Pos{};
		glm::vec4 Color{};
	};

	struct AuxiliaryLineData
	{
		glm::vec2* Head{ nullptr };
		glm::vec2* Tail{ nullptr };
		glm::vec4 Color{};
	};

	struct AuxiliaryForceData
	{
		glm::vec2* pos{ nullptr };
		glm::vec2* Force{ nullptr };
		glm::vec4 Color{};
	};

	struct AuxiliarySpotData
	{
		glm::vec2* pos{ nullptr };
		glm::vec4 Color{};
	};

	class AuxiliaryVision
	{
	public:
		AuxiliaryVision(Device* device, PipelineS* P, const unsigned int number);
		~AuxiliaryVision();

		//初始化描述符
		void initUniformManager(
			int frameCount, //GPU画布的数量
			std::vector<Buffer*> VPMstdBuffer//玩家相机的变化矩阵 
		);

		void RecordingCommandBuffer(RenderPass* R, SwapChain* S) {
			wRenderPass = R;
			wSwapChain = S;
			initCommandBuffer();
		}

		void initCommandBuffer();

		void GetCommandBuffer(std::vector<VkCommandBuffer>* Vector, unsigned int F) {
			Vector->push_back(mCommandBuffer[F]->getCommandBuffer());
		}

		void UpDataLine();

		auto GetContinuousLine() { return ContinuousAuxiliaryLine; }
		auto GetContinuousForce() { return ContinuousAuxiliaryForce; }
		void AddLine(glm::vec2 Vertex1, glm::vec2 Vertex2, glm::vec4 color) {
			LineVertex.push_back({ { Vertex1, 0.0f }, color });
			LineVertex.push_back({ { Vertex2, 0.0f }, color });
		};
		void AddSpot(glm::vec2 pos, glm::vec4 color) {
			SpotVertex.push_back({ { pos, 0.0f }, color });
		};
	private://线段
		unsigned int LineMax = 0;//上一帧的数量
		ContinuousMap<glm::vec2*, AuxiliaryLineData>* ContinuousAuxiliaryLine = nullptr;//两点连线
		ContinuousMap<glm::vec2*, AuxiliaryForceData>* ContinuousAuxiliaryForce = nullptr;//点上的向量
		std::vector<AuxiliaryLine> LineVertex;//单纯的位置连线（一次性）
		Buffer* AuxiliaryLineS{ nullptr };//线段缓存
	private://点
		unsigned int SpotMax = 0;//上一帧的数量
		ContinuousMap<glm::vec2*, AuxiliarySpotData>* ContinuousAuxiliarySpot = nullptr;//点集
		std::vector<AuxiliaryLine> SpotVertex;//单纯的位置点（一次性）
		Buffer* AuxiliarySpotS{ nullptr };//点缓存

	private:
		std::vector<UniformParameter*> mUniformParams{};

		DescriptorPool* mDescriptorPool{ nullptr };
		DescriptorSet* mDescriptorSetLine{ nullptr };
		DescriptorSet* mDescriptorSetSpot{ nullptr };
		CommandPool* mCommandPool{ nullptr };
		CommandBuffer** mCommandBuffer{ nullptr };

	private://储存
		const unsigned int Number;
		Device* wDevice{ nullptr };
		PipelineS* wPipelineS{ nullptr };
		SwapChain* wSwapChain{ nullptr };
		RenderPass* wRenderPass{ nullptr };
	};

}