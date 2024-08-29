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

		virtual ~FixedSizeTerrain();


		inline void SetPos(glm::vec2 pos) noexcept { PosX = pos.x; PosY = pos.y;}

		inline void SetPosX(float x) noexcept { PosX = x; }

		inline void SetPosY(float y) noexcept { PosY = y; }

		[[nodiscard]] inline float GetPosX() noexcept { return PosX; }

		[[nodiscard]] inline float GetPosY() noexcept { return PosY; }

	private:
		float PosX = 0;
		float PosY = 0;
	};

}