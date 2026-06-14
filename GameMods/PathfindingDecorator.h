#pragma once
#include "../PhysicsBlock/BaseStruct.hpp"

namespace GAME {

	class PathfindingDecorator
	{
	public:
		PathfindingDecorator(){}
		~PathfindingDecorator(){}

		int PathfindingDecoratorDeviationX = 0;
		int PathfindingDecoratorDeviationY = 0;

		virtual inline bool GetPixelWallNumber(unsigned int x, unsigned int y) = 0;

		virtual PhysicsBlock::CollisionInfoI RadialCollisionDetection(int x, int y, int Ex, int Ey) = 0;
	};
}