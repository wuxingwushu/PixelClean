#pragma once
#include "Particle/ParticleWorld.h"
#include "../Tool/ContinuousData.h"

namespace GAME {

	//粒子特效的结构体（Plan D Phase 5：移除 Pixel/Buffer 指针，改存 ParticleWorld 索引）
	struct SpecialEffects {
		float x{ 0.0f };
		float y{ 0.0f };
		float speed{ 0.0f };
		float angle{ 0.0f };
		float Zoom{ 1.0f };
		uint32_t particleIndex{ 0 };   // ParticleWorld 中的索引（替代旧的 Pixel/Buffer 指针）
	};

	class ParticlesSpecialEffect
	{
	public:
		//导入粒子世界（ParticleWorld facade），和粒子上限
		ParticlesSpecialEffect(ParticleWorld* world, unsigned int size);
		~ParticlesSpecialEffect();

		//生成一个粒子特效
		void GenerateSpecialEffects(double x, double y, unsigned char* colour, double angle, double speed);
		//销毁对应的粒子
		void DeleteSpecialEffects(unsigned int index);
		//粒子事件
		void SpecialEffectsEvent(unsigned int Fndex, double time);
	private:
		ContinuousData<SpecialEffects>* mSpecialEffects;//现场的粒子（仅存索引和元数据，无指针）
		ParticleWorld* mWorld;//粒子世界（facade）
	};

}
