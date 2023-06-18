#pragma once
#include "ParticleSystem.h"
#include "../Tool/ContinuousData.h"
#include "ParticlesSpecialEffect.h"
#include "../Physics/SquarePhysics.h"


namespace GAME {

	class Arms
	{
	public:
		Arms(ParticleSystem* particleSystem, unsigned int size);
		~Arms();

		void SetSquarePhysics(SquarePhysics::SquarePhysics* SquarePhysics) {
			mSquarePhysics = SquarePhysics;
		}

		void SetSpecialEffect(ParticlesSpecialEffect* particlesSpecialEffect) {
			mParticlesSpecialEffect = particlesSpecialEffect;
		}

		void ShootBullets(float x, float y, unsigned char* colour, float angle, float speed);

		//void BulletsEvent(float Stepping, SquarePhysics::SquarePhysics* LSquarePhysics);

		void BulletsEvent();

		void DeleteBullet(SquarePhysics::PixelCollision* index);

		ContinuousMap<SquarePhysics::PixelCollision*, Particle>* mBullet;
	private:
		ParticleSystem* mParticleSystem = nullptr;
		ParticlesSpecialEffect* mParticlesSpecialEffect = nullptr;
		SquarePhysics::SquarePhysics* mSquarePhysics = nullptr;
	};
}
