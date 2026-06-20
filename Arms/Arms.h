#pragma once
#include "Particle/ParticleWorld.h"
#include "../Tool/ContinuousData.h"
#include "../Tool/ContinuousMap.h"
#include "ParticlesSpecialEffect.h"
#include "../PhysicsBlock/PhysicsWorld.hpp"
#include "../PhysicsBlock/PhysicsParticle.hpp"
#include "AttackMode.h"
#include <set>




namespace GAME {

	// 子弹类型：决定子弹的反弹次数、是否爆炸、伤害表现
	enum class BulletTypeEnum
	{
		Pistol = 0,    // 手枪：反弹 3 次，命中坦克伤害 1 格
		Shrapnel = 1,  // 霰弹：不反弹，散弹
		MachineGun = 2,// 机枪：不反弹，高射速
		Rocket = 3,    // 火箭：不反弹，命中即范围爆炸
	};

	// 武器配置：每种武器的发射参数
	struct WeaponConfig {
		_ShootCallback Mode = nullptr; // 发射模式回调
		float Interval = 0.5f;         // 射击间隔（秒）
		int BounceCount = 0;           // 子弹反弹次数（0=不反弹）
		bool Explode = false;          // 是否爆炸弹
		float ExplodeRadius = 0.0f;    // 爆炸半径（格）
	};

	class Arms
	{
	public:

		Arms(ParticleWorld* world, unsigned int size);
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

		//生成子弹（按当前武器配置）
		void ShootBullets(double x, double y, double angle, double speed, unsigned int Type);

		void Shoot(double x, double y, double angle, double speed, unsigned int Type) {
			ShootCallback(this, x, y, angle, speed, Type);
		}

		//设置武器模式（同步反弹/爆炸参数）
		void SetArmsMode(AttackModeEnum mode) {
			AttackModeStruct M = GetAttackMode(mode);
			ShootCallback = M.Mode;
			IntervalTime = M.Interval;
			CurrentWeapon = mode;
			// 同步子弹配置
			const WeaponConfig& cfg = GetWeaponConfig(mode);
			mBulletBounceCount = cfg.BounceCount;
			mBulletExplode = cfg.Explode;
			mExplodeRadius = cfg.ExplodeRadius;
		}

		// 获取当前武器配置（供 UI 显示）
		AttackModeEnum GetCurrentWeapon() const { return CurrentWeapon; }
		int GetCurrentBounceCount() const { return mBulletBounceCount; }

		// 当前玩家坦克（用于爆炸伤害判定）；由 TankTrouble 设置
		void SetPlayerTank(class GamePlayer* player) { mPlayerTank = player; }
		void SetCrowd(class Crowd* crowd) { mCrowd = crowd; }

		// 获取武器配置（静态表）
		static WeaponConfig GetWeaponConfig(AttackModeEnum mode);

		// 注册子弹击中坦克时的销毁处理（每个坦克创建时调用一次）
		void RegisterTankBulletHandler(class GamePlayer* tank);

		//子弹事件
		void BulletsEvent();

		//销毁子弹（内部使用，会从物理世界正确移除）
		void DeleteBullet(PhysicsBlock::PhysicsParticle* index);

		//处理待删除子弹（在物理步进后调用，避免在回调中删除导致数据损坏）
		void ProcessPendingDeletions();

		// 爆炸：在 pos 处半径 radius 范围内破坏地形并对坦克造成伤害
		void Explode(glm::vec2 pos, float radius);

		ContinuousMap<PhysicsBlock::PhysicsParticle*, uint32_t>* mBullet;//子弹映射（物理粒子 -> ParticleWorld 索引）

	float IntervalTime = 0.5f;
private:
	_ShootCallback ShootCallback = PistolMode;
	ParticleWorld* mWorld = nullptr;//粒子世界（Plan D facade）
	ParticlesSpecialEffect* mParticlesSpecialEffect = nullptr;//粒子特效
		PhysicsBlock::PhysicsWorld* mSquarePhysics = nullptr;//物理系统
		std::set<PhysicsBlock::PhysicsParticle*> mPendingDeleteBullets;//待删除子弹集合

		AttackModeEnum CurrentWeapon = AttackModeEnum::Pistol;
		int mBulletBounceCount = 3;     // 当前武器的子弹反弹次数
		bool mBulletExplode = false;    // 当前武器子弹是否爆炸
		float mExplodeRadius = 0.0f;    // 当前武器的爆炸半径

		class GamePlayer* mPlayerTank = nullptr;
		class Crowd* mCrowd = nullptr;
	};
}
