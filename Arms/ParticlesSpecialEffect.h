#pragma once
#include "ParticleSystem.h"
#include "../Tool/ContinuousData.h"

#include "../Physics/SquarePhysics.h"

namespace GAME {

	//粒子特效的结构体
	struct SpecialEffects {
		float x;
		float y;
		float speed;
		float angle;
		float Zoom;
		PixelTexture* Pixel;
		std::vector<VulKan::Buffer*>* Buffer;
	};

	class ParticlesSpecialEffect
	{
	public:
		//导入粒子系统，和粒子上线
		ParticlesSpecialEffect(ParticleSystem* particleSystem, unsigned int size);
		~ParticlesSpecialEffect();

		//生成一个粒子特效
		void GenerateSpecialEffects(float x, float y, unsigned char* colour, float angle, float speed);
		//销毁对应的粒子
		void DeleteSpecialEffects(unsigned int index);
		//粒子事件
		void SpecialEffectsEvent(unsigned int Fndex, float time);
	private:
		ContinuousData<SpecialEffects>* mSpecialEffects;//现场的粒子
		ParticleSystem* mParticleSystem;//粒子系统
	};

}