#pragma once
#include "../Vulkan/buffer.h"
#include "../Vulkan/device.h"
#include "../Vulkan/descriptorSet.h"
#include "PipelineS.h"
#include "../Vulkan/swapChain.h"
#include "../Vulkan/renderPass.h"
#include "PixelTexture.h"
#include "../Tool/ContinuousMap.h"

namespace VulKan
{

	// 线基础单位
	struct AuxiliaryLineSpot
	{
		glm::vec3 Pos{};
		glm::vec4 Color{};
	};

	// 点基础单位
	struct AuxiliarySpot
	{
		glm::vec3 Pos{};
		float Size;
		glm::vec4 Color{};
	};

	// 圆基础单位
	struct AuxiliaryCircle
	{
		glm::vec3 Pos{};
		float Radius;
		glm::vec4 Color{};
	};

	struct AuxiliaryLineData
	{
		glm::dvec2 *Head{nullptr};
		glm::dvec2 *Tail{nullptr};
		glm::vec4 Color{};
	};

	struct AuxiliaryForceData
	{
		glm::dvec2 *pos{nullptr};
		glm::dvec2 *Force{nullptr};
		glm::vec4 Color{};
	};

	struct AuxiliarySpotData
	{
		glm::dvec2 *pos{nullptr};
		glm::vec4 Color{};
	};

	typedef unsigned int (*AuxiliarySpotDataHandle)(AuxiliarySpot *P, void *D, unsigned int Size);
	struct StaticAuxiliarySpotData
	{
		void *Pointer = nullptr;
		unsigned int Size = 0;
		AuxiliarySpotDataHandle Function = nullptr;
	};

	typedef unsigned int (*AuxiliaryLineDataHandle)(AuxiliaryLineSpot *P, void *D, unsigned int Size);
	struct StaticAuxiliaryLineData
	{
		void *Pointer = nullptr;
		unsigned int Size = 0;
		AuxiliaryLineDataHandle Function = nullptr;
	};

	class AuxiliaryVision
	{
	public:
		AuxiliaryVision(Device *device, PipelineS *P, const unsigned int number);
		~AuxiliaryVision();

		// 初始化描述符
		void initUniformManager(
			int frameCount,					   // GPU画布的数量
			std::vector<Buffer *> VPMstdBuffer // 玩家相机的变化矩阵
		);

		// 初次录制 CommandBuffer
		void RecordingCommandBuffer(RenderPass *R, SwapChain *S)
		{
			wRenderPass = R;
			wSwapChain = S;
			initCommandBuffer();
		}

		// 刷新 CommandBuffer
		void initCommandBuffer();

		// 获取某帧的 CommandBuffer
		void GetCommandBuffer(std::vector<VkCommandBuffer> *Vector, unsigned int F)
		{
			Vector->push_back(mCommandBuffer[F]->getCommandBuffer());
		}

		// 开始录制辅助视觉
		void Begin();

		// 画线
		inline void Line(glm::dvec3 Vertex1, glm::vec4 color1, glm::dvec3 Vertex2, glm::vec4 color2)
		{
			LinePointerHOST->Pos = Vertex1;
			LinePointerHOST->Color = color1;
			++LinePointerHOST;
			LinePointerHOST->Pos = Vertex2;
			LinePointerHOST->Color = color2;
			++LinePointerHOST;
			LineNumber += 2;
		};

		// 画点
		inline void Spot(glm::dvec3 pos, float size, glm::vec4 color)
		{
			SpotPointerHOST->Pos = pos;
			SpotPointerHOST->Size = size;
			SpotPointerHOST->Color = color;
			++SpotPointerHOST;
			++SpotNumber;
		};

		// 画圆 
		inline void Circle(glm::dvec3 pos, float radius, glm::vec4 color)
		{
			CirclePointerHOST->Pos = pos;
			CirclePointerHOST->Radius = radius;
			CirclePointerHOST->Color = color;
			++CirclePointerHOST;
			++CircleNumber;
		};

		// 结束录制辅助视觉
		void End();

		// 一次性线段（录制外使用）
		inline void AddLine(glm::dvec3 Vertex1, glm::dvec3 Vertex2, glm::vec4 color)
		{
			LineVertex.push_back({Vertex1, color});
			LineVertex.push_back({Vertex2, color});
		};
		// 一次性点（录制外使用）
		inline void AddSpot(glm::dvec3 pos, float size, glm::vec4 color)
		{
			SpotVertex.push_back({pos, size, color});
		};

		// 一次性点（录制外使用）
		inline void AddCircle(glm::dvec3 pos, float radius, glm::vec4 color)
		{
			CircleVertex.push_back({pos, radius, color});
		};

		// 静态线段（录制外使用）
		inline void AddStaticLine(glm::dvec3 Vertex1, glm::dvec3 Vertex2, glm::vec4 color)
		{
			StaticLineVertex.push_back({Vertex1, color});
			StaticLineVertex.push_back({Vertex2, color});
			StaticLineVertexUpData = true;
		};

		// 清空静态线段
		inline void ClearStaticLine() { StaticLineVertex.clear(); }

		// 静态点（录制外使用）
		inline void AddStaticSpot(glm::dvec3 pos, float size, glm::vec4 color)
		{
			StaticSpotVertex.push_back({pos, size, color});
			StaticSpotVertexUpData = true;
		};

		// 清空静态点
		inline void ClearStaticSpot() { StaticSpotVertex.clear(); }

		// 静态圆（录制外使用）
		inline void AddStaticCircle(glm::dvec3 pos, float radius, glm::vec4 color)
		{
			StaticCircleVertex.push_back({pos, radius, color});
			StaticCircleVertexUpData = true;
		};

		// 清空静态圆
		inline void ClearStaticCircle() { StaticCircleVertex.clear(); }

		// 动态线段
		inline constexpr auto GetContinuousLine() noexcept { return ContinuousAuxiliaryLine; }
		// 动态线段（力）
		inline constexpr auto GetContinuousForce() noexcept { return ContinuousAuxiliaryForce; }
		// 动态点
		inline constexpr auto GetContinuousSpot() noexcept { return ContinuousAuxiliarySpot; }

		// 静态线段
		inline constexpr auto GetContinuousStaticLine() noexcept { return StaticContinuousAuxiliaryLine; }
		// 静态点
		inline constexpr auto GetContinuousStaticSpot() noexcept { return StaticContinuousAuxiliarySpot; }
		// 静态数据更新
		inline constexpr void OpenStaticSpotUpData() noexcept { StaticSpotUpData = true; }
		inline constexpr void OpenStaticLineUpData() noexcept { StaticLineUpData = true; }

	private: // 线段
		AuxiliaryLineSpot *LinePointerHOST = nullptr;
		unsigned int LineNumber = 0;															 // 当前帧的数量
		unsigned int LineMax = 0;																 // 上一帧的数量
		ContinuousMap<glm::dvec2 *, AuxiliaryLineData> *ContinuousAuxiliaryLine = nullptr;		 // 两点连线（动态）
		ContinuousMap<glm::dvec2 *, AuxiliaryForceData> *ContinuousAuxiliaryForce = nullptr;	 // 点上的向量（动态）
		ContinuousMap<void *, StaticAuxiliaryLineData> *StaticContinuousAuxiliaryLine = nullptr; // 线段集（静态）
		bool StaticLineUpData = false;															 // 静态线段是否需要更新
		unsigned int StaticLineDeviation = 0;													 // 静态数据偏移量
		std::vector<AuxiliaryLineSpot> LineVertex{};											 // 单纯的位置连线（一次性）
		bool StaticLineVertexUpData = false;													 // 静态线段是否需要更新
		unsigned int StaticLineVertexDeviation = 0;												 // 静态数据偏移量
		std::vector<AuxiliaryLineSpot> StaticLineVertex{};										 // 单纯的位置连线（静态）
		Buffer *AuxiliaryLineS{nullptr};														 // 线段缓存

	private: // 点
		AuxiliarySpot *SpotPointerHOST = nullptr;
		unsigned int SpotNumber = 0;															 // 当前帧的数量
		unsigned int SpotMax = 0;																 // 上一帧的数量
		ContinuousMap<glm::dvec2 *, AuxiliarySpotData> *ContinuousAuxiliarySpot = nullptr;		 // 点集（动态）
		ContinuousMap<void *, StaticAuxiliarySpotData> *StaticContinuousAuxiliarySpot = nullptr; // 点集（静态）
		bool StaticSpotUpData = false;															 // 静态点是否需要更新
		unsigned int StaticSpotDeviation = 0;													 // 静态数据偏移量
		std::vector<AuxiliarySpot> SpotVertex{};												 // 单纯的位置点（一次性）
		bool StaticSpotVertexUpData = false;													 // 静态点是否需要更新
		unsigned int StaticSpotVertexDeviation = 0;												 // 静态数据偏移量
		std::vector<AuxiliarySpot> StaticSpotVertex{};											 // 单纯的位置点（静态）
		Buffer *AuxiliarySpotS{nullptr};														 // 点缓存

	private: // 圆
		AuxiliaryCircle *CirclePointerHOST = nullptr;
		unsigned int CircleNumber = 0;					   // 当前帧的数量
		unsigned int CircleMax = 0;						   // 上一帧的数量
		std::vector<AuxiliaryCircle> CircleVertex{};
		bool StaticCircleVertexUpData = false;			   // 静态点是否需要更新
		unsigned int StaticCircleVertexDeviation = 0;	   // 静态数据偏移量
		std::vector<AuxiliaryCircle> StaticCircleVertex{}; // 单纯的位置点（静态）
		Buffer *AuxiliaryCircleS{nullptr};				   // 圆缓存

	private:
		DescriptorPool *mDescriptorPool{nullptr};
		DescriptorSet *mDescriptorSetLine{nullptr};
		DescriptorSet *mDescriptorSetSpot{nullptr};
		DescriptorSet *mDescriptorSetCircle{nullptr};
		CommandPool *mCommandPool{nullptr};
		CommandBuffer **mCommandBuffer{nullptr};

	private: // 储存
		const unsigned int Number;
		Device *wDevice{nullptr};
		PipelineS *wPipelineS{nullptr};
		SwapChain *wSwapChain{nullptr};
		RenderPass *wRenderPass{nullptr};
	};

}
