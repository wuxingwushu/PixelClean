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

	struct StaticVertexTracker
	{
		bool UpData = false;
		unsigned int FirstNewIndex = 0;
	};

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
		static constexpr unsigned int kVertexCountBuffer = 100;

		AuxiliaryVision(Device *device, PipelineS *P, const unsigned int number);
		~AuxiliaryVision();

		// 初始化描述符
		void initUniformManager(
			int frameCount,					   // GPU画布的数量
			std::vector<Buffer *> VPMstdBuffer // 玩家相机的变化矩阵
		);

		// 初次录制 CommandBuffer
		// 注意：命令缓冲区是静态录制的（bindDescriptorSet 记录了 DescriptorSet 句柄引用），
		// Buffer 内容通过 HostVisible 内存直接更新，无需重新录制 CommandBuffer 即可反映变化
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
		bool End();

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
			if (!mLineStaticVertex.UpData) {
				mLineStaticVertex.FirstNewIndex = (unsigned int)StaticLineVertex.size();
			}
			StaticLineVertex.push_back({Vertex1, color});
			StaticLineVertex.push_back({Vertex2, color});
			mLineStaticVertex.UpData = true;
		};

		// 清空静态线段
		inline void ClearStaticLine() {
			StaticLineVertex.clear();
			mLineStaticVertex.FirstNewIndex = 0;
			mLineStaticVertex.UpData = true;
		}

		// 静态点（录制外使用）
		inline void AddStaticSpot(glm::dvec3 pos, float size, glm::vec4 color)
		{
			if (!mSpotStaticVertex.UpData) {
				mSpotStaticVertex.FirstNewIndex = (unsigned int)StaticSpotVertex.size();
			}
			StaticSpotVertex.push_back({pos, size, color});
			mSpotStaticVertex.UpData = true;
		};

		// 清空静态点
		inline void ClearStaticSpot() {
			StaticSpotVertex.clear();
			mSpotStaticVertex.FirstNewIndex = 0;
			mSpotStaticVertex.UpData = true;
		}

		// 静态圆（录制外使用）
		inline void AddStaticCircle(glm::dvec3 pos, float radius, glm::vec4 color)
		{
			if (!mCircleStaticVertex.UpData) {
				mCircleStaticVertex.FirstNewIndex = (unsigned int)StaticCircleVertex.size();
			}
			StaticCircleVertex.push_back({pos, radius, color});
			mCircleStaticVertex.UpData = true;
		};

		// 清空静态圆
		inline void ClearStaticCircle() {
			StaticCircleVertex.clear();
			mCircleStaticVertex.FirstNewIndex = 0;
			mCircleStaticVertex.UpData = true;
		}

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
		inline constexpr void OpenStaticSpotUpData() noexcept { mStaticSpotCallbackUpData = true; }
		inline constexpr void OpenStaticLineUpData() noexcept { mStaticLineCallbackUpData = true; }

	private: // 线段
		AuxiliaryLineSpot *LinePointerHOST = nullptr;
		unsigned int LineNumber = 0;
		unsigned int LineMax = 0;
		unsigned int mLineRecordedCount = 0;
		ContinuousMap<glm::dvec2 *, AuxiliaryLineData> *ContinuousAuxiliaryLine = nullptr;
		ContinuousMap<glm::dvec2 *, AuxiliaryForceData> *ContinuousAuxiliaryForce = nullptr;
		ContinuousMap<void *, StaticAuxiliaryLineData> *StaticContinuousAuxiliaryLine = nullptr;
		StaticVertexTracker mLineStaticVertex{};
		bool mStaticLineCallbackUpData = false;
		unsigned int StaticLineDeviation = 0;
		std::vector<AuxiliaryLineSpot> LineVertex{};
		unsigned int StaticLineVertexDeviation = 0;
		std::vector<AuxiliaryLineSpot> StaticLineVertex{};
		Buffer *AuxiliaryLineS{nullptr};
		AuxiliaryLineSpot *mLinePersistentPtr{nullptr};

	private: // 点
		AuxiliarySpot *SpotPointerHOST = nullptr;
		unsigned int SpotNumber = 0;
		unsigned int SpotMax = 0;
		unsigned int mSpotRecordedCount = 0;
		ContinuousMap<glm::dvec2 *, AuxiliarySpotData> *ContinuousAuxiliarySpot = nullptr;
		ContinuousMap<void *, StaticAuxiliarySpotData> *StaticContinuousAuxiliarySpot = nullptr;
		StaticVertexTracker mSpotStaticVertex{};
		bool mStaticSpotCallbackUpData = false;
		unsigned int StaticSpotDeviation = 0;
		std::vector<AuxiliarySpot> SpotVertex{};
		unsigned int StaticSpotVertexDeviation = 0;
		std::vector<AuxiliarySpot> StaticSpotVertex{};
		Buffer *AuxiliarySpotS{nullptr};
		AuxiliarySpot *mSpotPersistentPtr{nullptr};

	private: // 圆
		AuxiliaryCircle *CirclePointerHOST = nullptr;
		unsigned int CircleNumber = 0;
		unsigned int CircleMax = 0;
		unsigned int mCircleRecordedCount = 0;
		std::vector<AuxiliaryCircle> CircleVertex{};
		StaticVertexTracker mCircleStaticVertex{};
		unsigned int StaticCircleVertexDeviation = 0;
		std::vector<AuxiliaryCircle> StaticCircleVertex{};
		Buffer *AuxiliaryCircleS{nullptr};
		AuxiliaryCircle *mCirclePersistentPtr{nullptr};

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
