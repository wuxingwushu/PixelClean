#include "ParticleWorld.h"
#include "../Vulkan/buffer.h"
#include "../Vulkan/device.h"
#include "../Vulkan/commandPool.h"
#include "../Vulkan/pipeline.h"
#include "../Vulkan/renderPass.h"
#include "../Vulkan/swapChain.h"
#include "../Vulkan/descriptorSet.h"
#include "../Vulkan/descriptorPool.h"
#include "../Vulkan/description.h"
#include "../GlobalStructural.h"
#include "../GlobalVariable.h"
#include "../DebugLog.h"

namespace GAME {

ParticleWorld::ParticleWorld(VulKan::Device* device,
                             VulKan::CommandPool* commandPool,
                             uint32_t frameCount,
                             uint32_t maxParticles)
	: mDevice(device), mCommandPool(commandPool), mFrameCount(frameCount),
	  mMaxParticles(maxParticles), mPool(maxParticles) {
	LOGI("ParticleWorld::ParticleWorld(frameCount=%u, maxParticles=%u)", frameCount, maxParticles);

	// CPU 数据固定大小 = maxParticles
	mCPUData.resize(maxParticles);

	// 创建四组件
	mEmitter  = std::make_unique<ParticleEmitter>(mPool, mCPUData);
	mUpdater  = std::make_unique<ParticleUpdater>(mPool, mCPUData);
	mRenderer = std::make_unique<ParticleRenderer>(device, commandPool, frameCount, maxParticles);

	// 创建共享四边形几何体（与旧 ParticleSystem 一致）
	float positions[12] = {
		-0.5f, -0.5f, 0.0f,
		 0.5f, -0.5f, 0.0f,
		 0.5f,  0.5f, 0.0f,
		-0.5f,  0.5f, 0.0f,
	};
	float uvs[8] = {
		1.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,
	};
	unsigned int indices[6] = {
		0, 1, 2,
		2, 3, 0,
	};
	mPositionBuffer = VulKan::Buffer::createVertexBuffer(device, 12 * sizeof(float), positions);
	mUVBuffer       = VulKan::Buffer::createVertexBuffer(device, 8 * sizeof(float), uvs);
	mIndexBuffer    = VulKan::Buffer::createIndexBuffer(device, 6 * sizeof(unsigned int), indices);
	mIndexCount     = 6;

	// 把几何体传给 Renderer
	mRenderer->setGeometry(mPositionBuffer, mUVBuffer, mIndexBuffer, mIndexCount);

	LOGI("ParticleWorld: all components created");
}

ParticleWorld::~ParticleWorld() {
	// 等待 GPU 空闲
	if (mDevice) {
		vkDeviceWaitIdle(mDevice->getDevice());
	}

	delete mDescriptorSet;
	delete mDescriptorPool;

	delete mPositionBuffer;
	delete mUVBuffer;
	delete mIndexBuffer;

	// unique_ptr 自动释放 Emitter/Updater/Renderer
	// Renderer 析构释放 SSBO 和 CommandBuffer
	// Pool/CPUData 是栈成员，自动释放
}

int32_t ParticleWorld::emit(float x, float y, const unsigned char colour[4],
                            float angle, float speed) {
	return mEmitter->emit(x, y, colour, angle, speed);
}

int32_t ParticleWorld::emitBullet(float x, float y, const unsigned char colour[4],
                                  float angle, float speed, void* physicsParticle) {
	return mEmitter->emitBullet(x, y, colour, angle, speed, physicsParticle);
}

void ParticleWorld::kill(uint32_t index) {
	if (index >= mMaxParticles) return;
	if (!mCPUData[index].alive) return;
	mCPUData[index].alive = false;
	mPool.free(index);
}

void ParticleWorld::update(double time) {
	mUpdater->update(time, [this](uint32_t index) {
		// 死亡回调：Updater 已经 free 了索引并标记 alive=false
		// 这里可以做额外的清理（如通知 Arms）
		(void)index;
	});
}

void ParticleWorld::initDescriptorSet(std::vector<VulKan::Buffer*> vpBuffers,
                                      VkDescriptorSetLayout descriptorSetLayout) {
	LOGI("ParticleWorld::initDescriptorSet(frameCount=%u)", mFrameCount);

	// 创建 2 个 UniformParameter：binding 0 = VP UBO, binding 1 = Instance SSBO
	std::vector<VulKan::UniformParameter*> params;
	params.resize(2);

	// binding 0: VP UBO（外部传入的每帧 VP 矩阵 buffer）
	VulKan::UniformParameter* vpParam = new VulKan::UniformParameter();
	vpParam->mBinding = 0;
	vpParam->mCount = 1;
	vpParam->mDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpParam->mSize = sizeof(VPMatrices);
	vpParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;
	vpParam->mBuffers = vpBuffers;
	params[0] = vpParam;

	// binding 1: Instance SSBO（Renderer 的每帧实例 buffer）
	VulKan::UniformParameter* ssboParam = new VulKan::UniformParameter();
	ssboParam->mBinding = 1;
	ssboParam->mCount = 1;
	ssboParam->mDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	ssboParam->mSize = mMaxParticles * sizeof(ParticleInstanceData);
	ssboParam->mStage = VK_SHADER_STAGE_VERTEX_BIT;
	for (uint32_t i = 0; i < mFrameCount; ++i) {
		ssboParam->mBuffers.push_back(mRenderer->getInstanceBuffer(i));
	}
	params[1] = ssboParam;

	// 创建 DescriptorPool（size = frameCount 个 set）
	mDescriptorPool = new VulKan::DescriptorPool(mDevice);
	mDescriptorPool->build(params, mFrameCount, 1);

	// 创建 DescriptorSet（frameCount 个，每帧一个）
	mDescriptorSet = new VulKan::DescriptorSet(mDevice, params, descriptorSetLayout, mDescriptorPool, mFrameCount);

	// 释放临时 params（DescriptorSet 已复制所需信息）
	delete vpParam;
	delete ssboParam;

	LOGI("ParticleWorld: DescriptorSet created");
}

void ParticleWorld::recordCommandBuffer(VulKan::RenderPass* rp, VulKan::SwapChain* sc, VulKan::Pipeline* pipe) {
	mRenderPass = rp;
	mSwapChain  = sc;
	mPipeline   = pipe;
	// 初始录制一个带滞后余量的容量，并请求主 CB 更新以引用粒子二级 CB 句柄
	mRenderer->initialRecord(rp, sc, pipe, mDescriptorSet);
	Global::MainCommandBufferUpdateRequest();
}

void ParticleWorld::updateGPU(uint32_t frameIndex) {
	uint32_t count = activeCount();
	// 1. 每帧更新 SSBO（持久映射，无需重录）
	mRenderer->uploadInstanceData(mCPUData, count, frameIndex);
	// 2. 阈值检查，仅跨阈值时重录全部帧二级 CB 并请求主 CB 更新（参考 AuxiliaryVision::End）
	mRenderer->checkAndReRecord(count, mRenderPass, mSwapChain, mPipeline, mDescriptorSet);
}

void ParticleWorld::getCommandBuffers(std::vector<VkCommandBuffer>* out, uint32_t frameIndex) {
	// 仅推送句柄：上传/录制已在 updateGPU 中完成
	// 始终推送（活跃数为 0 时靠 uploadInstanceData 中的隐藏槽位处理）
	mRenderer->getCommandBuffer(out, frameIndex);
}

void ParticleWorld::resetCommandPools() {
	mRenderer->resetCommandPools();
}

} // namespace GAME
