#pragma once
#include "ParticleSystem.h"
#include "../Tool/ContinuousData.h"
#include "ParticlesSpecialEffect.h"
#include "../PhysicsBlock/PhysicsWorld.hpp"
#include "../PhysicsBlock/PhysicsParticle.hpp"
#include "AttackMode.h"
#include <set>




namespace GAME {

	class Arms
	{
	public:

		Arms(ParticleSystem* particleSystem, unsigned int size);
		~Arms();

		//获取物理系统
		void SetSquarePhysics(PhysicsBlock::PhysicsWorld* PhysicsWorld) {
			mSquarePhysics = PhysicsWorld;
		}

		//获取粒子特效
		void SetSpecialEffect(ParticlesSpecialEffect* particlesSpecialEffect) {
			mParticlesSpecialEffect = particlesSpecialEffect;
		}

		PhysicsBlock::PhysicsWorld* GetSquarePhysics() {
			return mSquarePhysics;
		}

		//生成子弹
		void ShootBullets(double x, double y, double angle, double speed, unsigned int Type);

		void Shoot(double x, double y, double angle, double speed, unsigned int Type) {
			ShootCallback(this, x, y, angle, speed, Type);
		}

		void SetArmsMode(AttackModeEnum mode) {
			AttackModeStruct M = GetAttackMode(mode);
			ShootCallback = M.Mode;
			IntervalTime = M.Interval;
		}

		//子弹事件
		void BulletsEvent();

		//销毁子弹（内部使用，会从物理世界正确移除）
		void DeleteBullet(PhysicsBlock::PhysicsParticle* index);

		//处理待删除子弹（在物理步进后调用，避免在回调中删除导致数据损坏）
		void ProcessPendingDeletions();

		ContinuousMap<PhysicsBlock::PhysicsParticle*, Particle>* mBullet;//子弹映射

		float IntervalTime = 0.5f;
	private:
		_ShootCallback ShootCallback = PistolMode;
		ParticleSystem* mParticleSystem = nullptr;//粒子系统
		ParticlesSpecialEffect* mParticlesSpecialEffect = nullptr;//粒子特效
		PhysicsBlock::PhysicsWorld* mSquarePhysics = nullptr;//物理系统
		std::set<PhysicsBlock::PhysicsParticle*> mPendingDeleteBullets;//待删除子弹集合
	};
}
