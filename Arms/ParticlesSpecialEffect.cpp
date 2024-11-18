#include "ParticlesSpecialEffect.h"
#include "../GlobalStructural.h"


namespace GAME {

	ParticlesSpecialEffect::ParticlesSpecialEffect(ParticleSystem* particleSystem, unsigned int size)
	{
		mParticleSystem = particleSystem;
		mSpecialEffects = new ContinuousData<SpecialEffects>(size);
	}

	ParticlesSpecialEffect::~ParticlesSpecialEffect()
	{
		for (auto& i : *mSpecialEffects)
		{
			mParticleSystem->mParticle->add(Particle{ i.Pixel, i.Buffer });
		}
		delete mSpecialEffects;
	}

	void ParticlesSpecialEffect::GenerateSpecialEffects(double x, double y, unsigned char* colour, double angle, double speed) {
		Particle* LParticle = mParticleSystem->mParticle->pop();//从粒子系统获取一个闲置的粒子
		if (LParticle == nullptr)
		{
			return;
		}
		SpecialEffects LSpecialEffects;
		LSpecialEffects.x = x;
		LSpecialEffects.y = y;
		LSpecialEffects.angle = angle;
		LSpecialEffects.speed = speed;
		LSpecialEffects.Zoom = 1.0f;
		LSpecialEffects.Pixel = LParticle->Pixel;
		LSpecialEffects.Buffer = LParticle->Buffer;
		LSpecialEffects.Pixel->ModifyImage(4, (void*)colour);//上传颜色
		mSpecialEffects->add(LSpecialEffects);//将创建的粒子特效添加到数组中
		ObjectUniform mUniform;
		mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
		mUniform.mModelMatrix = glm::rotate(mUniform.mModelMatrix, (float)glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));
		mUniform.mModelMatrix = glm::scale(mUniform.mModelMatrix, glm::vec3(LSpecialEffects.Zoom, LSpecialEffects.Zoom, 1.0f));//缩放矩阵
		for (auto i : *LSpecialEffects.Buffer)
		{
			i->updateBufferByMap((void*)&mUniform, sizeof(ObjectUniform));//上传位置
		}
	}

	void ParticlesSpecialEffect::DeleteSpecialEffects(unsigned int index) {
		SpecialEffects* LSpecialEffects = mSpecialEffects->GetData(index);//获取对应的粒子特效
		ObjectUniform mUniform;
		mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -10000.0f));
		for (auto i : *LSpecialEffects->Buffer)
		{
			i->updateBufferByMap((void*)&mUniform, sizeof(ObjectUniform));//设置到看不见的位置
		}
		mParticleSystem->mParticle->add(Particle{ LSpecialEffects->Pixel, LSpecialEffects->Buffer });//将粒子归还给粒子系统
		mSpecialEffects->Delete(index);//销毁对应的粒子
	}

	void ParticlesSpecialEffect::SpecialEffectsEvent(unsigned int Fndex, double time) {
		SpecialEffects* LmSpecialEffects = mSpecialEffects->Data();//获取粒子特效数组
		ObjectUniform mUniform;
		for (auto i = 0; i < mSpecialEffects->GetNumber(); ++i)
		{
			//更新位置，和大小
			LmSpecialEffects[i].x += LmSpecialEffects[i].speed * time * cos(LmSpecialEffects[i].angle);
			LmSpecialEffects[i].y += LmSpecialEffects[i].speed * time * sin(LmSpecialEffects[i].angle);
			LmSpecialEffects[i].Zoom -= time * 0.3f;
			if (LmSpecialEffects[i].Zoom < 0) {//如果大小为负，那就销毁这个粒子
				DeleteSpecialEffects(i);
			}
			mUniform.mModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(LmSpecialEffects[i].x, LmSpecialEffects[i].y, 0.0f));
			mUniform.mModelMatrix = glm::rotate(mUniform.mModelMatrix, (float)glm::radians(LmSpecialEffects[i].angle), glm::vec3(0.0, 0.0, 1.0));
			mUniform.mModelMatrix = glm::scale(mUniform.mModelMatrix, glm::vec3(LmSpecialEffects[i].Zoom, LmSpecialEffects[i].Zoom, 1.0));//缩放矩阵
			(*LmSpecialEffects[i].Buffer)[Fndex]->updateBufferByMap((void*)&mUniform, sizeof(ObjectUniform));//更新位置大小
		}
	}
}