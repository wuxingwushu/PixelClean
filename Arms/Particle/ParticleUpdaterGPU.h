#pragma once
// 粒子系统重构 - 方案 D
// GPU 更新器（占位，Phase 9 实现）
// 首期不实现，仅保留接口。当 Phase 8 后确有性能瓶颈时再启动。

#include "ParticleCommon.h"
#include "ParticlePool.h"
#include <vector>
#include <functional>

namespace GAME {

class ParticleUpdaterGPU {
public:
	using DeathCallback = std::function<void(uint32_t index)>;

	ParticleUpdaterGPU(ParticlePool& pool, std::vector<ParticleCPUData>& cpuData)
		: mPool(pool), mCPUData(cpuData) {}

	// 占位实现：首期不实现 GPU Compute 更新
	// Phase 9 将实现：新增 particle.comp，位置/缩放/生命周期更新 + dead list（atomic counter）
	void update(double time, DeathCallback onDeath) {
		// GPU Updater 首期不实现，空实现
		(void)time;
		(void)onDeath;
	}

private:
	ParticlePool&                 mPool;
	std::vector<ParticleCPUData>& mCPUData;
};

} // namespace GAME
