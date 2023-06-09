#pragma once
#include <cmath>
#include "StructuralComponents.h"
#include "FixedSizeTerrain.h"
#include "ObjectCollision.h"
#include "PixelCollision.h"
#include "../Tool/ContinuousData.h"
#include <iostream>

// 移位 > 赋值 > 大小比较 > 加法 > 减法 > 乘法 > 取模 > 除法。

namespace SquarePhysics {


	class SquarePhysics
	{
	public:
		SquarePhysics(unsigned int ObjectNumber, unsigned int PixelNumber);
		~SquarePhysics();

		void SetPhysicsSimulationStep(float step) { 
			if (step <= 0)
			{
				std::cerr << "SetPhysicsSimulationStep[Error]: 不可以设置小于等于 0 的值！" << std::endl;
				return;
			}
			mStep = step; 
		}

		void AddObjectCollision(ObjectCollision* LObjectCollision) { mObjectCollisionS->add(LObjectCollision); }

		void AddPixelCollision(PixelCollision* LPixelCollision) { mPixelCollisionS->add(LPixelCollision); }

		void SetFixedSizeTerrain(FixedSizeTerrain* LFixedSizeTerrain) { mFixedSizeTerrain = LFixedSizeTerrain; }

		void PhysicsSimulation(float TimeStep);

		bool PixelCollisionPhysicsSimulation(float TimeStep, PixelCollision* LPixelCollision);

	private:
		float mStep = 1.0f;
		FixedSizeTerrain* mFixedSizeTerrain = nullptr;
		ContinuousData<ObjectCollision*>* mObjectCollisionS = nullptr;
		ContinuousData<PixelCollision*>* mPixelCollisionS = nullptr;
	};

	//vec2旋转(基于原点的旋转)
	glm::dvec2 vec2angle(glm::dvec2 pos, double angle);
	//vec2旋转(基于原点的旋转)
	glm::dvec2 vec2angle(glm::dvec2 pos, double CosAngle, double SinAngle);


	//正方形和正方形的碰撞检测
	glm::dvec2 SquareToSquare(glm::vec2 posA, unsigned int dA, double angleA, glm::vec2 posB, unsigned int dB, double angleB);
	//正方形和点的碰撞检测
	glm::dvec2 SquareToDrop(float A1, float A2, float B1, float B2, glm::dvec2 Drop, glm::dvec2 PY);
}
