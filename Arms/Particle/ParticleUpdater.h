#pragma once
// 粒子系统重构 - 方案 D
// CPU 更新器：复刻旧 SpecialEffectsEvent 的运动学
#include "ParticleCommon.h"
#include "ParticlePool.h"
#include <vector>
#include <functional>

namespace GAME {

class ParticleUpdater {
public:
	using DeathCallback = std::function<void(uint32_t index)>;
	// 子弹位置同步回调：传入 physicsParticle 指针，返回 {x, y, angle}
	using BulletSyncCallback = std::function<void(void* physicsParticle, float& outX, float& outY, float& outAngle)>;

	ParticleUpdater(ParticlePool& pool, std::vector<ParticleCPUData>& cpuData);

	// 每帧调用
	// time: 帧时间（秒）
	// onDeath: 粒子死亡时回调（Emitter/Renderer 用它清理）
	void update(double time, DeathCallback onDeath);

	// 设置子弹位置同步回调（Phase 6：子弹跟随物理粒子）
	void setBulletSyncCallback(BulletSyncCallback callback) { mBulletSync = callback; }

private:
	ParticlePool&                 mPool;
	std::vector<ParticleCPUData>& mCPUData;
	BulletSyncCallback            mBulletSync;
};

} // namespace GAME
