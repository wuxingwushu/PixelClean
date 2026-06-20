#include "ParticleUpdater.h"
#include "../DebugLog.h"
#include <cmath>

namespace GAME {

ParticleUpdater::ParticleUpdater(ParticlePool& pool, std::vector<ParticleCPUData>& cpuData)
	: mPool(pool), mCPUData(cpuData) {
}

void ParticleUpdater::update(double time, DeathCallback onDeath) {
	// 遍历所有活跃粒子，更新位置/缩放/生命周期
	// 修复旧 bug：死亡时用 swap-and-pop 策略，循环用 while(i < n) 且死亡时不 ++i
	// （因为末尾元素交换到了 i 位置，需重新检查）
	//
	// 注意：Pool 的 freeList 是 LIFO，activeCount 无法直接对应连续索引。
	// 这里采用"收集活跃索引 + 反向遍历删除"的方式，保证正确性。
	// 收集当前所有活跃粒子的索引
	std::vector<uint32_t> activeIndices;
	activeIndices.reserve(mPool.activeCount());
	for (uint32_t i = 0; i < mCPUData.size(); ++i) {
		if (mCPUData[i].alive) {
			activeIndices.push_back(i);
		}
	}

	// 反向遍历，死亡时安全删除（不影响前面未遍历的索引）
	for (size_t k = activeIndices.size(); k > 0; --k) {
		uint32_t idx = activeIndices[k - 1];
		ParticleCPUData& d = mCPUData[idx];

		if (d.type == ParticleType::Bullet) {
			// 子弹：从外部物理粒子同步位置/角度
			if (mBulletSync && d.physicsParticle) {
				float bx, by, bangle;
				mBulletSync(d.physicsParticle, bx, by, bangle);
				d.x = bx;
				d.y = by;
				d.angle = bangle;
			}
			// 子弹不因 zoom 衰减而死亡，由 Arms::DeleteBullet 显式 kill
		} else {
			// 特效：自由运动学（复刻旧 SpecialEffectsEvent）
			// 旧代码：x += speed * time * cos(angle), y += speed * time * sin(angle)
			d.x += d.speed * static_cast<float>(time) * std::cos(d.angle);
			d.y += d.speed * static_cast<float>(time) * std::sin(d.angle);
			// 旧代码：Zoom -= time * 0.3f
			d.zoom -= static_cast<float>(time) * d.zoomDecay;

			if (d.zoom < 0) {
				// 死亡：标记 + 归还池 + 回调
				d.alive = false;
				mPool.free(idx);
				if (onDeath) {
					onDeath(idx);
				}
			}
		}
	}
}

} // namespace GAME
