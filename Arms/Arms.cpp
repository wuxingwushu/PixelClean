#include "Arms.h"


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
			delete mBullet->GetIndexKey(i);
		}
		delete mBullet;
	}

	void Arms::ShootBullets(float x, float y, unsigned char* colour, float angle, float speed) {
		SquarePhysics::PixelCollision* LPixelCollision = new SquarePhysics::PixelCollision(1);
		Particle* Ppppx = mBullet->New(LPixelCollision);
		*Ppppx = *mParticleSystem->mParticle->pop();
		LPixelCollision->SetPos({ x,y });
		LPixelCollision->SetSpeed(speed, angle);
		LPixelCollision->SetCollisionCallback(Arms_DeleteBullet,this);
		LPixelCollision->SetFrictionCoefficient(0.0f);
		Ppppx->Pixel->ModifyImage(4, colour);
		ObjectUniform mUniform;
		mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3( x, y, 0.0f));
		mUniform.mModelMatrix = glm::rotate(mUniform.mModelMatrix, glm::radians(angle * 180 / 3.14f), glm::vec3(0.0f, 0.0f, 1.0f));
		//mUniform.mModelMatrix = glm::scale(mUniform.mModelMatrix, glm::vec3(1.30, 1.30, 1.30));//缩放矩阵
		for (size_t i = 0; i < (*Ppppx->Buffer).size(); i++)
		{
			(*Ppppx->Buffer)[i]->updateBufferByMap(&mUniform, sizeof(ObjectUniform));
		}
		mSquarePhysics->AddPixelCollision(LPixelCollision);
		/*if (Ppppx == nullptr)
		{
			std::cout << "子弹耗光" << std::endl;
			return;
		}*/
	}

	void Arms::DeleteBullet(SquarePhysics::PixelCollision* index) {
		unsigned char color[4] = { 0, 0, 255, 125 };
		mParticlesSpecialEffect->GenerateSpecialEffects(index->GetPosX(), index->GetPosY(), color, index->GetSpeedAngleFloat() - 3.14f, 1.0f);
		//std::cout << index->GetSpeedAngleFloat() << std::endl;

		mParticleSystem->mParticle->add(
			Particle{
				mBullet->Get(index)->Pixel,
				mBullet->Get(index)->Buffer
			}
		);
		ObjectUniform mUniform;
		mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -1.0f));
		for (size_t i = 0; i < mBullet->Get(index)->Buffer->size(); i++)
		{
			(*mBullet->Get(index)->Buffer)[i]->updateBufferByMap((void*)(&mUniform), sizeof(ObjectUniform));
		}
		mBullet->Delete(index);
		delete index;
	}

	void Arms::BulletsEvent() {
		Particle* mParticledd = mBullet->GetData();
		SquarePhysics::PixelCollision* ppPixelCollision;
		for (size_t i = 0; i < mBullet->GetNumber(); i++)
		{
			ppPixelCollision = mBullet->GetIndexKey(i);
			ObjectUniform mUniform;
			mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(ppPixelCollision->GetPosX(), ppPixelCollision->GetPosY(), 0.0f));
			mUniform.mModelMatrix = glm::rotate(mUniform.mModelMatrix, glm::radians(ppPixelCollision->GetAngleFloat() * 180 / 3.14f), glm::vec3(0.0f, 0.0f, 1.0f));
			for (size_t idd = 0; idd < (*mParticledd->Buffer).size(); idd++)
			{
				(*mParticledd[i].Buffer)[idd]->updateBufferByMap(&mUniform, sizeof(ObjectUniform));
			}
		}
	}
}
