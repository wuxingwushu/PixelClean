#include "Arms.h"
#include "../Audio/SpatialAudio.h"
#include "../Audio/AudioEngine.h"
#include "../GlobalVariable.h"
#include "../GlobalStructural.h"
#include "../DebugLog.h"


namespace GAME {

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
	}

	void Arms::ShootBullets(double x, double y, double angle, double speed, unsigned int Type) {

		unsigned char color[4] = { 0,255,0,125 };

		Audio::AudioEngine::Get().GetSpatial().PlaySimple("Pistol", Audio::SimpleSoundType::MP3, false, Global::SoundEffectsVolume);

		//std::cout << "X: " << x << "Y: " << y << "angle: " << angle << std::endl;
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
		// 设置地形碰撞回调：碰撞时销毁地形并标记子弹待删除
		// 注意：不在回调中直接删除子弹，避免破坏物理引擎正在迭代的数据结构
		LPixelCollision->SetTerrainHitCallback(
			[this](glm::ivec2 hitPos, FLOAT_ angle, PhysicsBlock::PhysicsFormwork* bullet, void* userData) {
				LOGD("[Bullet] TerrainHitCallback: hitPos=(%d, %d), bullet=%p", hitPos.x, hitPos.y, bullet);
				PhysicsBlock::MapFormwork* map = mSquarePhysics->GetMapFormwork();
				if (map) {
					hitPos += glm::ivec2(map->FMGetCentrality());
					bool result = map->SafeSetCollision(hitPos, false);
					LOGD("[Bullet] SafeSetCollision: gridPos=(%d, %d), result=%d",
						hitPos.x, hitPos.y, result);
				}
				// 标记子弹待删除（延迟到 BulletsEvent 中通过 RemoveObject 安全移除）
				PhysicsBlock::PhysicsParticle* particle = (PhysicsBlock::PhysicsParticle*)bullet;
				particle->invMass = 0;  // 停止移动，避免本帧继续位移
				mPendingDeleteBullets.insert(particle);
				LOGD("[Bullet] PendingDelete: bullet=%p, pendingCount=%zu", particle, mPendingDeleteBullets.size());
			},
			this
		);
		Ppppx->Pixel->ModifyImage(4, color);//设置模型颜色
		ObjectUniform mUniform;
		mUniform.mModelMatrix = glm::translate(glm::mat4(1.0), glm::vec3( x, y, 0.0));//位置矩阵
		mUniform.mModelMatrix = glm::rotate(mUniform.mModelMatrix, (float)glm::radians(angle * 180.0 / 3.14), glm::vec3(0.0, 0.0, 1.0));//旋转矩阵
		//mUniform.mModelMatrix = glm::scale(mUniform.mModelMatrix, glm::vec3(1.30, 1.30, 1.30));//缩放矩阵
		for (size_t i = 0; i < (*Ppppx->Buffer).size(); ++i)
		{
			(*Ppppx->Buffer)[i]->updateBufferByMap(&mUniform, sizeof(ObjectUniform));//上传位置信息
		}
		mSquarePhysics->AddObject(LPixelCollision);//添加到物理系统中
	}

	void Arms::DeleteBullet(PhysicsBlock::PhysicsParticle* index) {
		LOGD("[Bullet] DeleteBullet: index=%p, pos=(%.1f, %.1f)", index, index->pos.x, index->pos.y);
		// 保存数据（RemoveObject 会 delete 粒子，需提前保存）
		Vec2_ pos = index->pos;
		Vec2_ speed = index->speed;
		Particle* particleData = mBullet->Get(index);
		
		// 生成销毁特效
		unsigned char color[4] = { 0, 0, 255, 125 };
		mParticlesSpecialEffect->GenerateSpecialEffects(pos.x, pos.y, color, atan2f(speed.y, speed.x) - 3.14f, 1.0f);
		
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
