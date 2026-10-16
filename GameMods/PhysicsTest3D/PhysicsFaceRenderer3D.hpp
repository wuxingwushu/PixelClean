#pragma once
#include <vector>
#include "../../Vulkan/buffer.h"
#include "../../Vulkan/commandBuffer.h"
#include "../../Vulkan/commandPool.h"
#include "../../Vulkan/descriptorPool.h"
#include "../../Vulkan/descriptorSet.h"
#include "../../GameMods/Configuration.h"
#include "../../PhysicsBlock3D/RenderMode3D.hpp"
#include "../../PhysicsBlock3D/PhysicsFaceMesh3D.hpp"

namespace PhysicsBlock3D
{
	class PhysicsWorld3D;
	class MapFormwork3D;
}

/*
 * ═══════════════════════════════════════════════════════════════════════════
 * 3D 物理的"面"渲染器
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * 需求（2026-10，用户 m00611）：物体与地形各有"线/面"两种渲染方式。线框由
 * `PhysicsAuxiliaryVision3D` 的 Line 管线负责，本类是"面"的那一半。
 *
 * 实现取舍：**不新建管线、不写新着色器**，直接复用
 * `PipelineMods::BlockWorldMods`——它的顶点输入恰好就是
 * 「位置 vec3 + 颜色 vec4 + 法线 vec3」（40 字节），binding 0 是
 * `VPMatrices`（相机 view+projection），与 `PhysicsBlock3D/PhysicsFaceMesh3D`
 * 生成的三角形完全对味。这样"面模式看起来和 BlockWorld 一样"是结构性保证，
 * 而不是靠调参逼近。
 *
 * 顶点缓冲是 CPU 每帧写入的 HOST_VISIBLE 缓冲（与 `AuxiliaryVision` 的做法一致），
 * 布局：[地形面][物体面]，一次上传、一次 draw。
 *
 * 注意：本类只被 3D 物理测试模块使用，所以放在游戏层的
 * `GameMods/PhysicsTest3D/`，而不是物理库 `PhysicsBlock3D/`。
 */
class PhysicsFaceRenderer3D
{
public:
	explicit PhysicsFaceRenderer3D(GAME::Configuration *wConfig);
	~PhysicsFaceRenderer3D();

	/** @brief 创建顶点缓冲/描述符/指令缓冲。失败返回 false（调用方应据此关闭面模式） */
	bool Init();

	/** @brief 释放 Vulkan 资源（会先 vkDeviceWaitIdle） */
	void Destroy();

	/**
	 * @brief 通知"地形面数据已失效"
	 * @details 地形面只在需要时重建（默认静态地图只在首帧/换代时建一次）。
	 *          地图被编辑、动态地图板块移动、或者切场景时调用。
	 */
	void InvalidateTerrain() { mTerrainDirty = true; }

	/**
	 * @brief 每帧调用：重建需要重建的面几何，并把顶点上传到 GPU
	 * @param world 物理世界（可为空）
	 * @param modes 当前渲染方式状态（决定哪些物体/地形走面渲染）
	 */
	void Update(PhysicsBlock3D::PhysicsWorld3D *world, const PhysicsBlock3D::RenderModeState3D &modes);

	/**
	 * @brief 录制二级指令缓冲（每交换链图像一份）
	 * @param force 忽略"顶点数没变"的短路，强制重录
	 */
	void RecordCommandBuffers(bool force = false);

	/**
	 * @brief 把第 F 帧的面渲染指令交给主指令缓冲列表
	 * @details 与 `VulkanTool/AuxiliaryVision.h::GetCommandBuffer` 同构：
	 *          `Interface` 会把收集到的二级指令缓冲按顺序提交。
	 */
	void GetCommandBuffer(std::vector<VkCommandBuffer> *Vector, unsigned int F)
	{
		if (Vector != nullptr && F < mCommandBuffers.size() && mCommandBuffers[F] != nullptr)
			Vector->push_back(mCommandBuffers[F]->getCommandBuffer());
	}

	/** @brief 当前面顶点数（调试面板显示用） */
	[[nodiscard]] unsigned int GetVertexCount() const { return mDrawVertexCount; }

private:
	/** @brief 真正创建 Vulkan 资源（被 Init 包在 try/catch 里调用） */
	bool CreateResources();

	/** @brief 确保顶点缓冲能放下 need 个顶点；需要扩容时重建缓冲（并要求重录指令） */
	bool EnsureCapacity(size_t need);

	GAME::Configuration *mConfig{ nullptr };

	VulKan::Buffer *mVertexBuffer{ nullptr };
	VulKan::DescriptorPool *mDescriptorPool{ nullptr };
	VulKan::DescriptorSet *mDescriptorSet{ nullptr };
	VulKan::CommandPool *mCommandPool{ nullptr };
	std::vector<VulKan::CommandBuffer *> mCommandBuffers{};

	/** @brief 顶点缓冲容量（顶点个数） */
	size_t mVertexCapacity{ 0 };
	/** @brief 本帧要画的顶点数 */
	unsigned int mDrawVertexCount{ 0 };
	/** @brief 上次录制时的顶点数（用来判断是否需要重录） */
	unsigned int mRecordedVertexCount{ 0 };
	/** @brief 缓冲被重建 / 首次使用 → 必须重录 */
	bool mNeedRecord{ true };

	/** @brief 地形面几何（缓存；只在失效时重建） */
	std::vector<PhysicsBlock3D::FaceVertex3D> mTerrainVertices{};
	/** @brief 物体面几何（每帧重建，因为物体会动） */
	std::vector<PhysicsBlock3D::FaceVertex3D> mObjectVertices{};
	/** @brief 地形面是否需要重建 */
	bool mTerrainDirty{ true };
	/** @brief 上次构建地形面时的世界 / 地图指针（换了就重建） */
	PhysicsBlock3D::PhysicsWorld3D *mLastWorld{ nullptr };
	PhysicsBlock3D::MapFormwork3D *mLastMap{ nullptr };
	/** @brief 上一帧地形是否处于面模式（用于检测"面模式关闭→重新打开"） */
	bool mLastTerrainFaceDrawn{ false };
};
