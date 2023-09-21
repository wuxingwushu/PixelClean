#pragma once
#include "ParticleSystem.h"
#include "../Tool/ContinuousData.h"
#include "ParticlesSpecialEffect.h"
#include "../Physics/SquarePhysics.h"
#include "AttackMode.h"




namespace GAME {

	class Arms
	{
	public:

		Arms(ParticleSystem* particleSystem, unsigned int size);
		~Arms();

		//获取物理系统
		void SetSquarePhysics(SquarePhysics::SquarePhysics* SquarePhysics) {
			mSquarePhysics = SquarePhysics;
		}

		//获取粒子特效
		void SetSpecialEffect(ParticlesSpecialEffect* particlesSpecialEffect) {
			mParticlesSpecialEffect = particlesSpecialEffect;
		}

		SquarePhysics::SquarePhysics* GetSquarePhysics() {
			return mSquarePhysics;
		}

		//生成子弹
		void ShootBullets(float x, float y, float angle, float speed, unsigned int Type);

		void Shoot(float x, float y, float angle, float speed, unsigned int Type) {
			ShootCallback(this, x, y, angle, speed, Type);
		}

		void SetArmsMode(AttackModeEnum mode) {
			AttackModeStruct M = GetAttackMode(mode);
			ShootCallback = M.Mode;
			IntervalTime = M.Interval;
		}

		//子弹事件
		void BulletsEvent();

		//销毁子弹
		void DeleteBullet(SquarePhysics::PixelCollision* index);

		ContinuousMap<SquarePhysics::PixelCollision*, Particle>* mBullet;//子弹映射

		float IntervalTime = 0.5f;
	private:
		_ShootCallback ShootCallback = PistolMode;
		ParticleSystem* mParticleSystem = nullptr;//粒子系统
		ParticlesSpecialEffect* mParticlesSpecialEffect = nullptr;//粒子特效
		SquarePhysics::SquarePhysics* mSquarePhysics = nullptr;//物理系统
	};
}
