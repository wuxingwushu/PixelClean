#pragma once
#include "ParticleSystem.h"
#include "../Tool/ContinuousData.h"

#include "../Physics/SquarePhysics.h"


namespace GAME {
	struct Bullet {
		float X;
		float Y;
		float Angle;
		float Speed;
		SquarePhysics::PixelCollision* mPixelCollision;
		Particle Particle;
	};

	class Arms
	{
	public:
		Arms(ParticleSystem* particleSystem, unsigned int size);
		~Arms();

		void SetSquarePhysics(SquarePhysics::SquarePhysics* SquarePhysics) {
			mSquarePhysics = SquarePhysics;
		}

		void ShootBullets(float x, float y, unsigned char* colour, float angle, float speed);

		//void BulletsEvent(float Stepping, SquarePhysics::SquarePhysics* LSquarePhysics);

		void BulletsEvent();

		void DeleteBullet(SquarePhysics::PixelCollision* index);

		//ContinuousData<Bullet>* mBullet;
		ContinuousMap<SquarePhysics::PixelCollision*, Particle>* mBullet;
	private:
		ParticleSystem* mParticleSystem;

		SquarePhysics::SquarePhysics* mSquarePhysics = nullptr;
	};
}
