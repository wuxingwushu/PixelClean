#include "ParticleEmitter.h"
#include "../DebugLog.h"

namespace GAME {

ParticleEmitter::ParticleEmitter(ParticlePool& pool, std::vector<ParticleCPUData>& cpuData)
	: mPool(pool), mCPUData(cpuData) {
}

int32_t ParticleEmitter::emit(float x, float y, const unsigned char colour[4],
                              float angle, float speed) {
	int32_t index = mPool.allocate();
	if (index < 0) {
		return -1;  // 池满
	}

	ParticleCPUData& d = mCPUData[index];
	d.x = x;
	d.y = y;
	d.speed = speed;
	d.angle = angle;
	d.zoom = 1.0f;
	d.zoomDecay = 0.3f;  // 原硬编码 0.3f
	// 颜色从 unsigned char[4] 转 glm::vec4（除以 255.0f）
	d.color = glm::vec4(
		colour[0] / 255.0f,
		colour[1] / 255.0f,
		colour[2] / 255.0f,
		colour[3] / 255.0f
	);
	d.alive = true;
	d.type = ParticleType::Effect;
	d.physicsParticle = nullptr;

	return index;
}

int32_t ParticleEmitter::emitBullet(float x, float y, const unsigned char colour[4],
                                    float angle, float speed, void* physicsParticle) {
	int32_t index = mPool.allocate();
	if (index < 0) {
		return -1;  // 池满
	}

	ParticleCPUData& d = mCPUData[index];
	d.x = x;
	d.y = y;
	d.speed = speed;
	d.angle = angle;
	d.zoom = 1.0f;       // 子弹不缩放衰减
	d.zoomDecay = 0.0f;
	d.color = glm::vec4(
		colour[0] / 255.0f,
		colour[1] / 255.0f,
		colour[2] / 255.0f,
		colour[3] / 255.0f
	);
	d.alive = true;
	d.type = ParticleType::Bullet;
	d.physicsParticle = physicsParticle;

	return index;
}

} // namespace GAME
