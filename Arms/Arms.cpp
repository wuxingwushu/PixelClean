#include "Arms.h"
#include "../SoundEffect/SoundEffect.h"

namespace GAME {

	void Arms_DeleteBullet(SquarePhysics::PixelCollision* PPixelCollision, void* mclass) {
		Arms* mClass = (Arms*)mclass;
		mClass->DeleteBullet(PPixelCollision);
	}

	Arms::Arms(ParticleSystem* particleSystem, unsigned int size)
	{
		mParticleSystem = particleSystem;
		mBullet = new ContinuousMap<SquarePhysics::PixelCollision*, Particle>(size);
	}

	Arms::~Arms()
	{
		for (size_t i = 0; i < mBullet->GetDataSize(); i++)
		{
			delete mBullet->GetIndexKey(i);//销毁生成的物理模型
		}
		delete mBullet;
	}

	void Arms::ShootBullets(float x, float y, unsigned char* colour, float angle, float speed) {

		SoundEffect::SoundEffect::GetSoundEffect()->Play("Pistol", MP3);

		//std::cout << "X: " << x << "Y: " << y << "angle: " << angle << std::endl;
		SquarePhysics::PixelCollision* LPixelCollision = new SquarePhysics::PixelCollision(1);//生成子弹物理模型
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
		
		LPixelCollision->SetPos({ x,y });//设置位置
		LPixelCollision->SetSpeed(speed, angle);//设置速度，角度
		LPixelCollision->SetCollisionCallback(Arms_DeleteBullet,this);//销毁回调函数
		LPixelCollision->SetFrictionCoefficient(0.0f);//设置摩擦系数
		Ppppx->Pixel->ModifyImage(4, colour);//设置模型颜色
		ObjectUniform mUniform;
		mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3( x, y, 0.0f));//位置矩阵
		mUniform.mModelMatrix = glm::rotate(mUniform.mModelMatrix, glm::radians(angle * 180 / 3.14f), glm::vec3(0.0f, 0.0f, 1.0f));//旋转矩阵
		//mUniform.mModelMatrix = glm::scale(mUniform.mModelMatrix, glm::vec3(1.30, 1.30, 1.30));//缩放矩阵
		for (size_t i = 0; i < (*Ppppx->Buffer).size(); i++)
		{
			(*Ppppx->Buffer)[i]->updateBufferByMap(&mUniform, sizeof(ObjectUniform));//上传位置信息
		}
		mSquarePhysics->AddPixelCollision(LPixelCollision);//添加到物理系统中
	}

	void Arms::DeleteBullet(SquarePhysics::PixelCollision* index) {
		unsigned char color[4] = { 0, 0, 255, 125 };
		mParticlesSpecialEffect->GenerateSpecialEffects(index->GetPosX(), index->GetPosY(), color, index->GetSpeedAngleFloat() - 3.14f, 1.0f);//子弹销毁特效
		//std::cout << index->GetSpeedAngleFloat() << std::endl;
		mParticleSystem->mParticle->add(//粒子归还粒子系统
			Particle{
				mBullet->Get(index)->Pixel,
				mBullet->Get(index)->Buffer
			}
		);
		ObjectUniform mUniform;
		mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -1.0f));
		for (size_t i = 0; i < mBullet->Get(index)->Buffer->size(); i++)
		{
			(*mBullet->Get(index)->Buffer)[i]->updateBufferByMap((void*)(&mUniform), sizeof(ObjectUniform));//设置到看不见的位置
		}
		mBullet->Delete(index);//销毁子弹映射
		delete index;//销毁子弹
	}

	void Arms::BulletsEvent() {
		Particle* mParticledd = mBullet->GetData();//获取所有子弹模型
		SquarePhysics::PixelCollision* ppPixelCollision;
		for (size_t i = 0; i < mBullet->GetNumber(); i++)//遍历所有子弹
		{
			ppPixelCollision = mBullet->GetIndexKey(i);//获取子弹物理模型
			ObjectUniform mUniform;
			mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(ppPixelCollision->GetPosX(), ppPixelCollision->GetPosY(), 0.0f));
			mUniform.mModelMatrix = glm::rotate(mUniform.mModelMatrix, glm::radians(ppPixelCollision->GetAngleFloat() * 180 / 3.14f), glm::vec3(0.0f, 0.0f, 1.0f));
			for (size_t idd = 0; idd < (*mParticledd->Buffer).size(); idd++)
			{
				(*mParticledd[i].Buffer)[idd]->updateBufferByMap(&mUniform, sizeof(ObjectUniform));//更新模型的位置
			}
		}
	}
}
