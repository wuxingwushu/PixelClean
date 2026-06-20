#pragma once
// 粒子系统重构 - 方案 D
// ParticleWorld (facade)：持有 Pool/Emitter/Updater/Renderer 的所有权，
// 对外暴露与旧 ParticleSystem 兼容的接口，协调组件间数据流。
#include "ParticleCommon.h"
#include "ParticlePool.h"
#include "ParticleEmitter.h"
#include "ParticleUpdater.h"
#include "ParticleRenderer.h"
#include <vector>
#include <memory>

namespace VulKan { class Device; class CommandPool; class DescriptorSet; class Pipeline; class RenderPass; class SwapChain; class Buffer; class Sampler; class DescriptorPool; }

namespace GAME {

class ParticleWorld {
public:
	ParticleWorld(VulKan::Device* device,
	              VulKan::CommandPool* commandPool,
	              uint32_t frameCount,
	              uint32_t maxParticles = MAX_PARTICLES);
	~ParticleWorld();

	// ===== 对外兼容接口（与旧 ParticleSystem 对齐）=====
	// 供 ParticlesSpecialEffect 兼容层调用
	int32_t emit(float x, float y, const unsigned char colour[4],
	             float angle, float speed);
	// 供 Arms 子弹系统调用（Phase 6）
	int32_t emitBullet(float x, float y, const unsigned char colour[4],
	                   float angle, float speed, void* physicsParticle);
	void    kill(uint32_t index);
	void    update(double time);                    // 对应 SpecialEffectsEvent
	[[nodiscard]] uint32_t activeCount() const noexcept { return mPool.activeCount(); }
	[[nodiscard]] uint32_t freeCount() const noexcept { return mPool.freeCount(); }   // 兼容旧 mParticle->GetNumber()

	// 检查粒子是否活跃（供 ParticlesSpecialEffect 清理死亡条目）
	[[nodiscard]] bool isAlive(uint32_t index) const noexcept {
		return index < mMaxParticles && mCPUData[index].alive;
	}

	// 设置子弹位置同步回调（Phase 6）
	void setBulletSyncCallback(ParticleUpdater::BulletSyncCallback callback) {
		mUpdater->setBulletSyncCallback(callback);
	}

	// 获取 CPU 数据（供 Arms 直接修改子弹位置，Phase 6 方案 6A）
	std::vector<ParticleCPUData>& getCPUData() { return mCPUData; }

	// ===== 渲染接口（对应旧 ParticleSystem）=====
	// 初始化描述符集（VP UBO + Instance SSBO）
	// vpBuffers: 外部传入的每帧 VP 矩阵 buffer
	// descriptorSetLayout: 粒子专用 Pipeline 的 DescriptorSetLayout
	void initDescriptorSet(std::vector<VulKan::Buffer*> vpBuffers,
	                       VkDescriptorSetLayout descriptorSetLayout);

	void recordCommandBuffer(VulKan::RenderPass* rp, VulKan::SwapChain* sc, VulKan::Pipeline* pipe);
	void getCommandBuffers(std::vector<VkCommandBuffer>* out, uint32_t frameIndex);
	void resetCommandPools();

	// 每帧调用（fence 等待后、createCommandBuffers 前）：
	// 1. 每帧更新 SSBO（持久映射，无需重录）
	// 2. 阈值检查，仅跨阈值时重录全部帧二级 CB 并请求主 CB 更新（参考 AuxiliaryVision::End）
	void updateGPU(uint32_t frameIndex);

private:
	VulKan::Device*       mDevice{ nullptr };
	VulKan::CommandPool*  mCommandPool{ nullptr };
	uint32_t              mFrameCount{ 0 };
	uint32_t              mMaxParticles{ 0 };

	ParticlePool          mPool;
	std::vector<ParticleCPUData> mCPUData;   // 固定大小 = maxParticles
	std::unique_ptr<ParticleEmitter>  mEmitter;
	std::unique_ptr<ParticleUpdater>  mUpdater;
	std::unique_ptr<ParticleRenderer> mRenderer;

	// 渲染资源
	VulKan::DescriptorPool* mDescriptorPool{ nullptr };
	VulKan::DescriptorSet*  mDescriptorSet{ nullptr };

	// 共享几何体（四边形顶点/UV/索引）
	VulKan::Buffer* mPositionBuffer{ nullptr };
	VulKan::Buffer* mUVBuffer{ nullptr };
	VulKan::Buffer* mIndexBuffer{ nullptr };
	uint32_t        mIndexCount{ 0 };

	// 渲染状态（recordCommandBuffer 时存储）
	VulKan::RenderPass* mRenderPass{ nullptr };
	VulKan::SwapChain*  mSwapChain{ nullptr };
	VulKan::Pipeline*   mPipeline{ nullptr };
};

} // namespace GAME
