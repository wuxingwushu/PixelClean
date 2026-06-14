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

	Arms::Arms(ParticleSystem* particleSystem, unsigned int size)
	{
		LOGD("Arms::Arms() called");
		mParticleSystem = particleSystem;
		mBullet = new ContinuousMap<PhysicsBlock::PhysicsParticle*, Particle>(size, ContinuousMap_None);
	}

	Arms::~Arms()
	{
		for (size_t i = 0; i < mBullet->GetDataSize(); ++i)
		{
			delete mBullet->GetIndexKey(i);//销毁生成的物理模型
		}
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
		Particle* Ppppx = mBullet->New(LPixelCollision);//生成子弹
		if (Ppppx == nullptr) {//达到上线
			delete LPixelCollision;
			return;
		}
		Particle* LPpppx = mParticleSystem->mParticle->pop();//从粒子系统获取模型
		if (LPpppx == nullptr) {//粒子系统以空
			mBullet->Delete(LPixelCollision);
			delete LPixelCollision;
			return;
		}
		*Ppppx = *LPpppx;

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
			LPixelCollision->SetTerrainHitCallback(
				[this](glm::ivec2 hitPos, FLOAT_ hitAngle, PhysicsBlock::PhysicsFormwork* bullet, void* userData) {
					auto* particle = (PhysicsBlock::PhysicsParticle*)bullet;

					auto it = gBulletState.find(particle);
					if (it == gBulletState.end()) {
						// 状态丢失，直接销毁
						particle->invMass = 0;
						mPendingDeleteBullets.insert(particle);
						return;
					}

					BulletRuntime& rt = it->second;

					// 坐标系转换：hitPos 是碰撞接触点的【世界坐标】（地图中心在原点 0,0），
					// 而 SafeSetCollision / FMGetCollide(glm::ivec2) 需要【网格坐标】（原点在左上角）。
					// 转换关系：grid = world + centrality
					PhysicsBlock::MapFormwork* map = mSquarePhysics->GetMapFormwork();
					glm::ivec2 gridPos = hitPos;
					if (map) {
						glm::vec2 cen = map->FMGetCentrality();
						gridPos += glm::ivec2((int)cen.x, (int)cen.y);
					}

					// 爆炸弹：命中即爆炸，直接销毁
					if (rt.explode) {
						Explode(particle->pos, rt.explodeRadius);
						particle->invMass = 0;
						mPendingDeleteBullets.insert(particle);
						return;
					}

					// 还有反弹次数：反射速度，不破坏地形，不销毁子弹
					if (rt.bounceRemaining > 0) {
						// 简化法线估算：根据命中网格周围 4 邻接格的实心情况判定是水平墙还是垂直墙
						if (map) {
							// 检查命中网格在 x 方向和 y 方向的邻居是否为实心墙（用网格坐标）
							bool wallLeft  = map->FMGetCollide(glm::ivec2(gridPos.x - 1, gridPos.y));
							bool wallRight = map->FMGetCollide(glm::ivec2(gridPos.x + 1, gridPos.y));
							bool wallDown  = map->FMGetCollide(glm::ivec2(gridPos.x, gridPos.y - 1));
							bool wallUp    = map->FMGetCollide(glm::ivec2(gridPos.x, gridPos.y + 1));

							Vec2_ spd = particle->speed;
							// 若左右有墙，则碰撞面是垂直的，翻转 x 分量
							if (wallLeft || wallRight) {
								spd.x = -spd.x;
							}
							// 若上下有墙，则碰撞面是水平的，翻转 y 分量
							if (wallDown || wallUp) {
								spd.y = -spd.y;
							}
							// 若都没匹配（角落或孤立格），同时翻转两个分量
							if (!(wallLeft || wallRight) && !(wallDown || wallUp)) {
								spd.x = -spd.x;
								spd.y = -spd.y;
							}
							particle->speed = spd;
						} else {
							particle->speed = Vec2_{ -particle->speed.x, -particle->speed.y };
						}

						// 把子弹位置稍微沿新速度方向推离墙体，避免下一帧重复触发
						Vec2_ dir = particle->speed;
						float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
						if (len > 0.0001f) {
							particle->pos.x += dir.x / len * 0.5f;
							particle->pos.y += dir.y / len * 0.5f;
						}
						rt.bounceRemaining--;
						return;
					}

					// 反弹次数耗尽：破坏地形并销毁子弹（用网格坐标）
					if (map) {
						bool result = map->SafeSetCollision(gridPos, false);
						LOGD("[Bullet] SafeSetCollision: worldPos=(%d,%d) gridPos=(%d,%d) result=%d",
							hitPos.x, hitPos.y, gridPos.x, gridPos.y, result);
					}
					particle->invMass = 0;  // 停止移动
					mPendingDeleteBullets.insert(particle);
					LOGD("[Bullet] PendingDelete: bullet=%p, pendingCount=%zu", particle, mPendingDeleteBullets.size());
				},
				this
			);
		Ppppx->Pixel->ModifyImage(4, color);//设置模型颜色
		ObjectUniform mUniform;
		mUniform.mModelMatrix = glm::translate(glm::mat4(1.0), glm::vec3( x, y, 0.0));//位置矩阵
		mUniform.mModelMatrix = glm::rotate(mUniform.mModelMatrix, (float)glm::radians(angle * 180.0 / 3.14), glm::vec3(0.0, 0.0, 1.0));//旋转矩阵
		for (size_t i = 0; i < (*Ppppx->Buffer).size(); ++i)
		{
			(*Ppppx->Buffer)[i]->updateBufferByMap(&mUniform, sizeof(ObjectUniform));//上传位置信息
		}
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

		// 对坦克造成伤害（按距离衰减）
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
		Particle* particleData = mBullet->Get(index);

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

		// 粒子归还粒子系统
		if (particleData) {
			mParticleSystem->mParticle->add(
				Particle{
					particleData->Pixel,
					particleData->Buffer
				}
			);
			// 设置到看不见的位置
			ObjectUniform mUniform;
			mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -10000.0f));
			for (size_t i = 0; i < particleData->Buffer->size(); ++i)
			{
				(*particleData->Buffer)[i]->updateBufferByMap((void*)(&mUniform), sizeof(ObjectUniform));
			}
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

		Particle* mParticledd = mBullet->GetData();//获取所有子弹模型
		PhysicsBlock::PhysicsParticle* ppPixelCollision;
		for (size_t i = 0; i < mBullet->GetNumber(); ++i)//遍历所有子弹
		{
			ppPixelCollision = mBullet->GetIndexKey(i);//获取子弹物理模型
			ObjectUniform mUniform;
			mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(ppPixelCollision->pos.x, ppPixelCollision->pos.y, 0.0f));
			mUniform.mModelMatrix = glm::rotate(mUniform.mModelMatrix, (float)glm::radians(atan2f(ppPixelCollision->speed.y, ppPixelCollision->speed.x) * 180 / 3.14), glm::vec3(0.0, 0.0, 1.0));
			for (size_t idd = 0; idd < (*mParticledd->Buffer).size(); ++idd)
			{
				(*mParticledd[i].Buffer)[idd]->updateBufferByMap(&mUniform, sizeof(ObjectUniform));//更新模型的位置
			}
		}
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
