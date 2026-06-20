#include "ParticlesSpecialEffect.h"
#include "../DebugLog.h"


namespace GAME {

	ParticlesSpecialEffect::ParticlesSpecialEffect(ParticleWorld* world, unsigned int size)
	{
		mWorld = world;
		mSpecialEffects = new ContinuousData<SpecialEffects>(size);
	}

	ParticlesSpecialEffect::~ParticlesSpecialEffect()
	{
		LOGD("ParticlesSpecialEffect::~ParticlesSpecialEffect() called");
		// Plan D：ParticleWorld 管理自己的索引池，无需手动归还粒子
		delete mSpecialEffects;
	}

	void ParticlesSpecialEffect::GenerateSpecialEffects(double x, double y, unsigned char* colour, double angle, double speed) {
		// 委托给 ParticleWorld 发射粒子（Type::Effect）
		int32_t index = mWorld->emit(static_cast<float>(x), static_cast<float>(y),
		                             colour, static_cast<float>(angle), static_cast<float>(speed));
		if (index < 0) {
			return;  // 池满
		}

		// 在 mSpecialEffects 中记录索引和元数据（元数据仅用于跟踪，实际状态由 ParticleWorld 管理）
		SpecialEffects effect;
		effect.x = static_cast<float>(x);
		effect.y = static_cast<float>(y);
		effect.angle = static_cast<float>(angle);
		effect.speed = static_cast<float>(speed);
		effect.Zoom = 1.0f;
		effect.particleIndex = static_cast<uint32_t>(index);
		mSpecialEffects->add(effect);
	}

	void ParticlesSpecialEffect::DeleteSpecialEffects(unsigned int index) {
		SpecialEffects* effect = mSpecialEffects->GetData(index);
		// 委托给 ParticleWorld 销毁粒子（若已死亡则为 no-op）
		mWorld->kill(effect->particleIndex);
		mSpecialEffects->Delete(index);
	}

	void ParticlesSpecialEffect::SpecialEffectsEvent(unsigned int Fndex, double time) {
		// Plan D Phase 5：Fndex 语义已内化
		// 旧 SpecialEffectsEvent(Fndex, time) 的 Fndex 是当前帧索引，用于更新对应帧的 UBO。
		// 新架构下 Renderer 在 getCommandBuffers(frameIndex) 时按帧上传 SSBO，
		// 故 ParticleWorld::update(time) 不需要 Fndex。
		(void)Fndex;

		// 1. 更新所有粒子（运动学 + 生命周期管理由 ParticleUpdater 负责）
		//    ParticleUpdater 内部用反向遍历修复了旧 SpecialEffectsEvent 的迭代 bug
		mWorld->update(time);

		// 2. 清理已死亡的粒子条目（反向遍历，安全删除）
		//    ParticleWorld::update 会自动 kill 死亡粒子（zoom<0），但 mSpecialEffects
		//    需要同步清理对应的跟踪条目
		for (int i = static_cast<int>(mSpecialEffects->GetNumber()) - 1; i >= 0; --i) {
			SpecialEffects* effect = mSpecialEffects->GetData(static_cast<unsigned int>(i));
			if (!mWorld->isAlive(effect->particleIndex)) {
				DeleteSpecialEffects(static_cast<unsigned int>(i));
			}
		}
	}
}
