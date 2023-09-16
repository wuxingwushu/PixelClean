#pragma once
#include "StructuralComponents.h"
#include "GridDecorator.h"


namespace SquarePhysics {

	class FixedSizeTerrain :public GridDecorator
	{
	public:
		FixedSizeTerrain(
			unsigned int x, 
			unsigned int y, 
			unsigned int SideLength);

		~FixedSizeTerrain();



		void SetPosX(float x) { PosX = x; }

		void SetPosY(float y) { PosY = y; }


		[[nodiscard]] float GetPosX() { return PosX; }

		[[nodiscard]] float GetPosY() { return PosY; }



	private:
		float PosX = 0;
		float PosY = 0;
	};

}