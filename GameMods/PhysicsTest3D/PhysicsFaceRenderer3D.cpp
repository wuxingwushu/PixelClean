#include "PhysicsFaceRenderer3D.hpp"
#include "../../GlobalStructural.h"
#include "../../DebugLog.h"
#include "../../PhysicsBlock3D/PhysicsWorld3D.hpp"
#include "../../PhysicsBlock3D/MapFormwork3D.hpp"
#include "../../PhysicsBlock3D/PhysicsAuxiliaryVision3D.hpp"
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <exception>

/*
 * ═══════════════════════════════════════════════════════════════════════════
 * PhysicsFaceRenderer3D 实现
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * 创建流程照抄 `GameMods/BlockWorld/BlockWorld.cpp` 的 `InitBlockWorldPipeline`
 * + `RecordBlockWorldCommandBuffers`（用的是同一条管线），只把顶点缓冲换成
 * "每帧 CPU 写"的可映射缓冲，并像 `VulkanTool/AuxiliaryVision.cpp::initCommandBuffer`
 * 那样录制二级指令缓冲。
 *
 * 所有 Vulkan 相关调用都包了 try/catch：本项目历史上出现过"模块构造里抛异常
 * 把整个程序带走"的问题（见 PhysicsBlock3D/ImGuiPhysics3D.cpp 的 ini 兜底注释），
 * 面渲染只是可选的显示方式，失败时应当退化成线框而不是崩掉。
 */

namespace
{
	/** @brief 顶点缓冲初始容量（顶点个数）：够 32×32 地图 + 若干物体，不够会自动扩容 */
	const size_t kInitialVertexCapacity3D = 1u << 16; // 65536 个面顶点

	/**
	 * @brief 地形面的颜色
	 * @details 与线框地形一致（`PhysicsAuxiliaryVision3D.cpp` 的
	 *          `RenderMapOutline3D` 里那个 `color`）——保持"线/面切换时颜色不变"。
	 */
	const glm::vec4 kTerrainFaceColor3D = { 0.0f, 1.0f, 0.0f, 1.0f };
}

PhysicsFaceRenderer3D::PhysicsFaceRenderer3D(GAME::Configuration *wConfig)
	: mConfig(wConfig)
{
}

PhysicsFaceRenderer3D::~PhysicsFaceRenderer3D()
{
	Destroy();
}

bool PhysicsFaceRenderer3D::Init()
{
	if (mConfig == nullptr || mConfig->mDevice == nullptr ||
	    mConfig->mPipelineS == nullptr || mConfig->mSwapChain == nullptr ||
	    mConfig->mRenderPass == nullptr)
		return false;
	if (mConfig->mCameraVPMatricesBuffer.empty())
		return false;

	try
	{
		return CreateResources();
	}
	catch (const std::exception &e)
	{
		LOGE("PhysicsFaceRenderer3D::Init 失败（将退化为纯线框）: %s", e.what());
		Destroy();
		return false;
	}
}

bool PhysicsFaceRenderer3D::CreateResources()
{
	const unsigned int frameCount = mConfig->mSwapChain->getImageCount();
	if (frameCount == 0)
		return false;

	// ── 顶点缓冲：HOST_VISIBLE + HOST_COHERENT，每帧直接映射写入 ──
	mVertexCapacity = kInitialVertexCapacity3D;
	mVertexBuffer = new VulKan::Buffer(
	    mConfig->mDevice,
	    sizeof(PhysicsBlock3D::FaceVertex3D) * mVertexCapacity,
	    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (mVertexBuffer == nullptr || mVertexBuffer->getBuffer() == VK_NULL_HANDLE)
		return false;

	// ── 描述符：binding 0 = 相机 VP 矩阵（BlockWorld 管线的布局就是这样）──
	VulKan::UniformParameter vpParam{};
	vpParam.mBinding = 0;
	vpParam.mCount = 1;
	vpParam.mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpParam.mSize = sizeof(VPMatrices);
	vpParam.mStage = VK_SHADER_STAGE_VERTEX_BIT;
	vpParam.mBuffers = mConfig->mCameraVPMatricesBuffer;

	std::vector<VulKan::UniformParameter *> paramsVector;
	paramsVector.push_back(&vpParam);

	auto *blockPipeline = mConfig->mPipelineS->GetPipeline(VulKan::PipelineMods::BlockWorldMods);
	if (blockPipeline == nullptr)
		return false;

	mDescriptorPool = new VulKan::DescriptorPool(mConfig->mDevice);
	mDescriptorPool->build(paramsVector, frameCount);
	mDescriptorSet = new VulKan::DescriptorSet(
	    mConfig->mDevice, paramsVector, blockPipeline->DescriptorSetLayout, mDescriptorPool, frameCount);

	// ── 二级指令缓冲：每交换链图像一份 ──
	mCommandPool = new VulKan::CommandPool(mConfig->mDevice);
	mCommandBuffers.resize(frameCount);
	for (unsigned int i = 0; i < frameCount; ++i)
		mCommandBuffers[i] = new VulKan::CommandBuffer(mConfig->mDevice, mCommandPool, true /*二级缓冲*/);

	mNeedRecord = true;
	return true;
}

void PhysicsFaceRenderer3D::Destroy()
{
	if (mConfig != nullptr && mConfig->mDevice != nullptr)
		vkDeviceWaitIdle(mConfig->mDevice->getDevice());

	for (VulKan::CommandBuffer *commandBuffer : mCommandBuffers)
		delete commandBuffer;
	mCommandBuffers.clear();

	delete mCommandPool;
	mCommandPool = nullptr;
	delete mDescriptorSet;
	mDescriptorSet = nullptr;
	delete mDescriptorPool;
	mDescriptorPool = nullptr;
	delete mVertexBuffer;
	mVertexBuffer = nullptr;

	mVertexCapacity = 0;
	mDrawVertexCount = 0;
	mRecordedVertexCount = 0;
	mTerrainVertices.clear();
	mObjectVertices.clear();
	mTerrainDirty = true;
	mLastWorld = nullptr;
	mLastMap = nullptr;
	mLastTerrainFaceDrawn = false;
}

bool PhysicsFaceRenderer3D::EnsureCapacity(size_t need)
{
	if (need <= mVertexCapacity)
		return true;

	// 按 1.5 倍逐次扩张，避免每加一个物体都重建
	size_t capacity = mVertexCapacity == 0 ? kInitialVertexCapacity3D : mVertexCapacity;
	while (capacity < need)
		capacity = capacity + capacity / 2 + 1;

	vkDeviceWaitIdle(mConfig->mDevice->getDevice()); // 缓冲可能还被上一帧使用
	delete mVertexBuffer;

	mVertexCapacity = capacity;
	mVertexBuffer = new VulKan::Buffer(
	    mConfig->mDevice,
	    sizeof(PhysicsBlock3D::FaceVertex3D) * mVertexCapacity,
	    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	mNeedRecord = true; // 绑定的缓冲换了，指令缓冲必须重录
	return mVertexBuffer != nullptr && mVertexBuffer->getBuffer() != VK_NULL_HANDLE;
}

void PhysicsFaceRenderer3D::Update(PhysicsBlock3D::PhysicsWorld3D *world,
                                   const PhysicsBlock3D::RenderModeState3D &modes)
{
	if (mVertexBuffer == nullptr)
		return;

	// ── 1. 物体面（每帧重建：物体会动）──
	mObjectVertices.clear();
	if (world != nullptr)
	{
		for (PhysicsBlock3D::PhysicsShape3D *obj : world->PhysicsShapeS)
		{
			if (obj == nullptr || modes.ObjectMode(obj) != PhysicsBlock3D::RenderMode3D::Face)
				continue;
			PhysicsBlock3D::BuildShapeFaces3D(obj, PhysicsBlock3D::ObjectBodyColor3D(obj->StaticNum), mObjectVertices);
		}
		for (PhysicsBlock3D::PhysicsCircle3D *obj : world->PhysicsCircleS)
		{
			if (obj == nullptr || modes.ObjectMode(obj) != PhysicsBlock3D::RenderMode3D::Face)
				continue;
			PhysicsBlock3D::BuildCircleFaces3D(obj, PhysicsBlock3D::ObjectBodyColor3D(obj->StaticNum), mObjectVertices);
		}
	}

	// ── 2. 地形面（缓存：静态地图不变，只在失效时重建）──
	const bool terrainFace = modes.TerrainMode() == PhysicsBlock3D::RenderMode3D::Face;
	PhysicsBlock3D::MapFormwork3D *map = world == nullptr ? nullptr : world->GetMapFormwork();
	if (terrainFace)
	{
		if (mTerrainDirty || mLastWorld != world || mLastMap != map || !mLastTerrainFaceDrawn)
		{
			mTerrainVertices.clear();
			PhysicsBlock3D::BuildMapFaces3D(world, kTerrainFaceColor3D, mTerrainVertices);
			mTerrainDirty = false;
		}
	}
	else if (mLastTerrainFaceDrawn)
	{
		mTerrainVertices.clear(); // 面模式关闭：不必再留着这份镜像
		mTerrainDirty = true;
	}
	mLastWorld = world;
	mLastMap = map;
	mLastTerrainFaceDrawn = terrainFace;

	// ── 3. 上传：[地形][物体]，一次映射写完 ──
	const size_t terrainCount = mTerrainVertices.size();
	const size_t objectCount = mObjectVertices.size();
	const size_t total = terrainCount + objectCount;
	mDrawVertexCount = (unsigned int)total;
	if (total == 0)
		return;
	if (!EnsureCapacity(total))
	{
		mDrawVertexCount = 0;
		return;
	}

	const size_t stride = sizeof(PhysicsBlock3D::FaceVertex3D);
	try
	{
		char *base = (char *)mVertexBuffer->getupdateBufferByMap();
		if (base == nullptr)
		{
			mDrawVertexCount = 0;
			return;
		}
		if (terrainCount > 0)
			memcpy(base, mTerrainVertices.data(), terrainCount * stride);
		if (objectCount > 0)
			memcpy(base + terrainCount * stride, mObjectVertices.data(), objectCount * stride);
		mVertexBuffer->endupdateBufferByMap();
	}
	catch (const std::exception &e)
	{
		LOGE("PhysicsFaceRenderer3D::Update 顶点上传失败: %s", e.what());
		mDrawVertexCount = 0;
		return;
	}

	if (mDrawVertexCount != mRecordedVertexCount)
		mNeedRecord = true;
}

void PhysicsFaceRenderer3D::RecordCommandBuffers(bool force)
{
	if (mVertexBuffer == nullptr || mDescriptorSet == nullptr ||
	    mConfig == nullptr || mConfig->mSwapChain == nullptr || mConfig->mRenderPass == nullptr)
		return;
	if (!force && !mNeedRecord && mDrawVertexCount == mRecordedVertexCount)
		return;

	auto *blockPipeline = mConfig->mPipelineS->GetPipeline(VulKan::PipelineMods::BlockWorldMods);
	if (blockPipeline == nullptr)
		return;

	VkCommandBufferInheritanceInfo inheritanceInfo{};
	inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	inheritanceInfo.renderPass = mConfig->mRenderPass->getRenderPass();
	inheritanceInfo.subpass = 0;

	for (unsigned int i = 0; i < mCommandBuffers.size(); ++i)
	{
		inheritanceInfo.framebuffer = mConfig->mSwapChain->getFrameBuffer(i);

		mCommandBuffers[i]->begin(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, inheritanceInfo);
		mCommandBuffers[i]->bindGraphicPipeline(blockPipeline->getPipeline());
		mCommandBuffers[i]->bindVertexBuffer({ mVertexBuffer->getBuffer() });
		mCommandBuffers[i]->bindDescriptorSet(blockPipeline->getLayout(), mDescriptorSet->getDescriptorSet((int)i));
		mCommandBuffers[i]->draw(mDrawVertexCount);
		mCommandBuffers[i]->end();
	}

	mRecordedVertexCount = mDrawVertexCount;
	mNeedRecord = false;
}
