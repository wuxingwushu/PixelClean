#pragma once
// 粒子系统重构 - 方案 D
// 发射器：负责粒子发射规则。当前实现"单点发射"（兼容旧 GenerateSpecialEffects）
#include "ParticleCommon.h"
#include "ParticlePool.h"
#include <vector>

namespace GAME {

class ParticleEmitter {
public:
	ParticleEmitter(ParticlePool& pool, std::vector<ParticleCPUData>& cpuData);

	// 兼容旧 GenerateSpecialEffects 的接口
	// colour 指向 4 字节 RGBA
	// 返回分配到的索引，-1 表示池满
	int32_t emit(float x, float y, const unsigned char colour[4],
	             float angle, float speed);

	// 发射子弹粒子（Type::Bullet），physicsParticle 为物理粒子指针
	int32_t emitBullet(float x, float y, const unsigned char colour[4],
	                   float angle, float speed, void* physicsParticle);

private:
	ParticlePool&                 mPool;
	std::vector<ParticleCPUData>& mCPUData;
};

} // namespace GAME
