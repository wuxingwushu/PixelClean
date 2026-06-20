#include "Arms.h"
#include "AttackMode.h"
#include "../Audio/SpatialAudio.h"
#include "../Audio/AudioEngine.h"
#include "../GlobalVariable.h"
#include "../GlobalStructural.h"
#include "../DebugLog.h"
#include "../Character/GamePlayer.h"
#include "../Character/Crowd.h"
#include <unordered_map>
#include <cmath>


namespace GAME {

	// 每颗子弹的运行时状态（反弹次数、是否爆炸）
	struct BulletRuntime {
		int bounceRemaining = 0;   // 剩余反弹次数
		bool explode = false;      // 是否爆炸弹
		float explodeRadius = 0;   // 爆炸半径
		unsigned int type = 0;     // 武器类型
	};

	// 全局子弹状态表（按子弹粒子指针索引）
	static std::unordered_map<PhysicsBlock::PhysicsParticle*, BulletRuntime> gBulletState;

	void Arms_DeleteBullet(PhysicsBlock::PhysicsParticle* PPixelCollision, void* mclass) {
		Arms* mClass = (Arms*)mclass;
		mClass->DeleteBullet(PPixelCollision);
	}

	Arms::Arms(ParticleWorld* world, unsigned int size)
	{
		LOGD("Arms::Arms() called, world=%p", world);
		mWorld = world;
		mBullet = new ContinuousMap<PhysicsBlock::PhysicsParticle*, uint32_t>(size, ContinuousMap_None);

		// 设置子弹位置同步回调：ParticleUpdater 在 update() 时调用，
		// 从物理粒子读取位置/角度写入 ParticleCPUData，供 Renderer 上传 SSBO
		if (mWorld) {
			mWorld->setBulletSyncCallback([](void* physicsParticle, float& outX, float& outY, float& outAngle) {
				auto* particle = static_cast<PhysicsBlock::PhysicsParticle*>(physicsParticle);
				outX = particle->pos.x;
				outY = particle->pos.y;
				outAngle = std::atan2(particle->speed.y, particle->speed.x);
			});
		}
	}

	Arms::~Arms()
	{
		// PhysicsWorld 析构时会清理所有 PhysicsParticle，此处不再重复释放
		delete mBullet;
		gBulletState.clear();
	}

	// 武器配置表：模式 -> 发射参数 + 子弹配置
	WeaponConfig Arms::GetWeaponConfig(AttackModeEnum mode) {
		switch (mode) {
			case AttackModeEnum::Pistol:     return { PistolMode,     0.5f, 3, false, 0.0f };  // 反弹 3 次
			case AttackModeEnum::Shrapnel:   return { ShrapnelMode,   1.0f, 0, false, 0.0f };  // 不反弹，5 发扇形
			case AttackModeEnum::MachineGun: return { MachineGunMode, 0.1f, 0, false, 0.0f };  // 不反弹，高射速散射
			case AttackModeEnum::Rocket:     return { RocketMode,     1.5f, 0, true,  5.0f };  // 命中即爆炸，半径 5 格
			default:                         return { PistolMode,     0.5f, 3, false, 0.0f };
		}
	}

	// 注册"坦克被子弹击中时销毁子弹"的处理（每个坦克创建时调用一次）
	void Arms::RegisterTankBulletHandler(GamePlayer* tank) {
		if (tank == nullptr) return;
		tank->BulletDestroyedHandler = [this](PhysicsBlock::PhysicsParticle* bullet) {
			// 停止子弹移动并标记待删除（在 BulletsEvent 中安全移除）
			bullet->invMass = 0;
			mPendingDeleteBullets.insert(bullet);
		};
	}

	void Arms::ShootBullets(double x, double y, double angle, double speed, unsigned int Type) {

		unsigned char color[4] = { 0,255,0,125 };

		Audio::AudioEngine::Get().GetSpatial().PlaySimple("Pistol", Audio::SimpleSoundType::MP3, false, Global::SoundEffectsVolume);

		PhysicsBlock::PhysicsParticle* LPixelCollision = new PhysicsBlock::PhysicsParticle(Vec2_{0, 0});//生成子弹物理模型
		LPixelCollision->mass = 1.0f;
		LPixelCollision->invMass = 1.0f / LPixelCollision->mass;

		// 在 ContinuousMap 中分配子弹映射槽位（物理粒子 -> ParticleWorld 索引）
		uint32_t* pIndex = mBullet->New(LPixelCollision);
		if (pIndex == nullptr) {//达到上限
			delete LPixelCollision;
			return;
		}

		// 向 ParticleWorld 发射子弹粒子（Type::Bullet），返回池中索引
		int32_t worldIndex = mWorld->emitBullet(
			static_cast<float>(x), static_cast<float>(y),
			color,
			static_cast<float>(angle), static_cast<float>(speed),
			LPixelCollision);
		if (worldIndex < 0) {//粒子池已满
			mBullet->Delete(LPixelCollision);
			delete LPixelCollision;
			return;
		}
		*pIndex = static_cast<uint32_t>(worldIndex);//存储 ParticleWorld 索引

		LPixelCollision->pos = Vec2_{static_cast<FLOAT_>(x), static_cast<FLOAT_>(y)};//设置位置
		LPixelCollision->speed = Vec2_{static_cast<FLOAT_>(cos(angle) * speed), static_cast<FLOAT_>(sin(angle) * speed)};//设置速度，角度
		LPixelCollision->friction = 0.0f;//设置摩擦系数

		// 记录该子弹的运行时状态（反弹次数、爆炸配置）
		BulletRuntime rt;
		rt.bounceRemaining = mBulletBounceCount;
		rt.explode = mBulletExplode;
		rt.explodeRadius = mExplodeRadius;
		rt.type = Type;
		gBulletState[LPixelCollision] = rt;

			// 设置地形碰撞回调：根据剩余反弹次数决定反弹还是破坏地形
			mSquarePhysics->GetCollision().AddTerrainHitListener(LPixelCollision,
			[this](const PhysicsBlock::BulletHitInfo& hitInfo, PhysicsBlock::PhysicsFormwork* bullet, void* userData) {
				auto* particle = (PhysicsBlock::PhysicsParticle*)bullet;

				auto it = gBulletState.find(particle);
				if (it == gBulletState.end()) {
					// 状态丢失，直接销毁
					particle->invMass = 0;
					mPendingDeleteBullets.insert(particle);
					return;
				}

				BulletRuntime& rt = it->second;

				// GridPos 已由 PhysicsCollision 计算好（= WorldPos + centrality），
				// 直接可当 SafeSetCollision / FMGetCollide 的地图网格坐标使用。
				PhysicsBlock::MapFormwork* map = mSquarePhysics->GetMapFormwork();
				glm::ivec2 gridPos = hitInfo.GridPos;

				// 爆炸弹：命中即爆炸，直接销毁
				if (rt.explode) {
					Explode(particle->pos, rt.explodeRadius);
					particle->invMass = 0;
					mPendingDeleteBullets.insert(particle);
					return;
				}

				// 还有反弹次数：使用精确法向量做镜面反射
				if (rt.bounceRemaining > 0) {
					Vec2_ spd = particle->speed;
					Vec2_ N = hitInfo.Normal;
					// 镜面反射：v' = v - 2*(v·n)*n
					float vn = spd.x * N.x + spd.y * N.y;
					spd.x -= 2.0f * vn * N.x;
					spd.y -= 2.0f * vn * N.y;
					particle->speed = spd;

					// 把子弹位置稍微沿新速度方向推离墙体，避免下一帧重复触发
					float len = std::sqrt(spd.x * spd.x + spd.y * spd.y);
					if (len > 0.0001f) {
						particle->pos.x += spd.x / len * 0.5f;
						particle->pos.y += spd.y / len * 0.5f;
					}
					rt.bounceRemaining--;
					return;
				}

				// 反弹次数耗尽：破坏地形并销毁子弹
				if (map) {
					bool result = map->SafeSetCollision(gridPos, false);
					LOGD("[Bullet] SafeSetCollision: gridPos=(%d,%d) result=%d",
						gridPos.x, gridPos.y, result);
				}
				particle->invMass = 0;  // 停止移动
				mPendingDeleteBullets.insert(particle);
				LOGD("[Bullet] PendingDelete: bullet=%p, pendingCount=%zu", particle, mPendingDeleteBullets.size());
			},
			this
		);
		mSquarePhysics->AddObject(LPixelCollision);//添加到物理系统中
	}

	// 爆炸：在 pos 处半径 radius（格）范围内破坏地形并对坦克造成伤害
	void Arms::Explode(glm::vec2 pos, float radius) {
		PhysicsBlock::MapFormwork* map = mSquarePhysics ? mSquarePhysics->GetMapFormwork() : nullptr;
		int r = (int)std::ceil(radius);
		glm::ivec2 center = map ? (glm::ivec2)(pos + map->FMGetCentrality()) : glm::ivec2((int)pos.x, (int)pos.y);

		// 破坏地形（圆形区域）
		if (map) {
			for (int dx = -r; dx <= r; ++dx) {
				for (int dy = -r; dy <= r; ++dy) {
					if (dx * dx + dy * dy <= r * r) {
						map->SafeSetCollision(glm::ivec2(center.x + dx, center.y + dy), false);
					}
				}
			}
		}

		// 爆炸特效
		if (mParticlesSpecialEffect) {
			unsigned char color[4] = { 255, 120, 0, 200 };
			mParticlesSpecialEffect->GenerateSpecialEffects(pos.x, pos.y, color, 0.0f, 1.5f);
		}

	// 对坦克造成伤害（按距离衰减） + 爆炸击飞（方案E）
	auto damageTank = [&](GamePlayer* tank) {
		if (tank == nullptr || tank->GetDeathInBattle()) return;
		glm::vec2 tpos = tank->GetObjectCollision()->pos;
		float dist = std::sqrt((tpos.x - pos.x) * (tpos.x - pos.x) + (tpos.y - pos.y) * (tpos.y - pos.y));
		if (dist > radius) return;
		// 计算爆炸中心相对坦克的网格坐标，在中心附近随机破坏若干格
		auto* shape = tank->GetObjectCollision();
		PhysicsBlock::CollisionInfoI info = shape->DropCollision(pos);
		int gx = info.pos.x;
		int gy = info.pos.y;
		int damageCells = (int)(3.0f * (1.0f - dist / radius)) + 1;
		for (int i = 0; i < damageCells; ++i) {
			int ox = gx + (rand() % 5) - 2;
			int oy = gy + (rand() % 5) - 2;
			if (ox >= 0 && ox < 16 && oy >= 0 && oy < 16) {
				tank->mPixelQueue->add({ ox, oy, false });
			}
		}

		// === 方案E：爆炸击飞 ===
		// 方向：从爆炸中心指向坦克（向外推），大小随距离衰减
		glm::vec2 kbDir = (dist > 1e-3f)
			? glm::vec2((tpos.x - pos.x) / dist, (tpos.y - pos.y) / dist)
			: glm::vec2(1, 0);
		float falloff = 1.0f - dist / radius;  // 0~1
		// 爆炸冲量（坦克质量≈256，5000 * falloff → 近处 Δv ≈ 19.5 px/s）
		const float explosionImpulse = 5000.0f * falloff;
		tank->GetObjectCollision()->ApplyImpulse(
			Vec2_{ kbDir.x * explosionImpulse, kbDir.y * explosionImpulse },
			Vec2_{ static_cast<FLOAT_>(pos.x), static_cast<FLOAT_>(pos.y) });
		if (tank->GetMovement()) {
			// 爆炸击飞时间更长：临时加大 RagdollMinTime 确保不被自己输入立刻覆盖
			tank->GetMovement()->Config().RagdollMinTime = 0.6f;
			tank->GetMovement()->SetMode(MovementMode::Ragdoll);
		}
	};

		damageTank(mPlayerTank);
		if (mCrowd) {
			mCrowd->ForEachPlayer(damageTank);
		}
	}

	void Arms::DeleteBullet(PhysicsBlock::PhysicsParticle* index) {
		LOGD("[Bullet] DeleteBullet: index=%p, pos=(%.1f, %.1f)", index, index->pos.x, index->pos.y);
		// 保存数据（RemoveObject 会 delete 粒子，需提前保存）
		Vec2_ pos = index->pos;
		Vec2_ speed = index->speed;

		// 从映射中取出 ParticleWorld 索引并 kill 粒子
		uint32_t* pWorldIndex = mBullet->Get(index);

		// 生成销毁特效（非爆炸弹才生成，避免爆炸时重复）
		auto it = gBulletState.find(index);
		bool isExplosive = (it != gBulletState.end() && it->second.explode);
		if (!isExplosive) {
			unsigned char color[4] = { 0, 0, 255, 125 };
			if (mParticlesSpecialEffect) {
				mParticlesSpecialEffect->GenerateSpecialEffects(pos.x, pos.y, color, atan2f(speed.y, speed.x) - 3.14f, 1.0f);
			}
		}
		gBulletState.erase(index);

		// 归还 ParticleWorld 池中的粒子
		if (pWorldIndex) {
			mWorld->kill(*pWorldIndex);
		}

		// 先从 ContinuousMap 中删除映射（必须在 RemoveObject 之前，因为 RemoveObject 会 delete index）
		mBullet->Delete(index);

		// 再从物理世界正确移除（清理网格搜索、碰撞组等所有引用，会 delete index）
		if (mSquarePhysics) {
			mSquarePhysics->RemoveObject(index);
		} else {
			delete index;
		}
	}

	void Arms::BulletsEvent() {
		// 先处理待删除的子弹（在物理步进后安全地调用 RemoveObject）
		ProcessPendingDeletions();
		// 子弹位置/角度同步由 ParticleWorld::update() 中的 BulletSyncCallback 完成，
		// Renderer 在 getCommandBuffers() 时根据 CPU 数据上传 SSBO，无需此处手动更新。
	}

	void Arms::ProcessPendingDeletions() {
		if (mPendingDeleteBullets.empty()) return;

		LOGD("[Bullet] ProcessPendingDeletions: count=%zu", mPendingDeleteBullets.size());
		for (auto* particle : mPendingDeleteBullets) {
			DeleteBullet(particle);
		}
		mPendingDeleteBullets.clear();
	}
}
