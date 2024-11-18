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


		inline void SetPos(glm::dvec2 pos) noexcept { Pos = pos; }

		inline void SetPosX(double x) noexcept { Pos.x = x; }

		inline void SetPosY(double y) noexcept { Pos.y = y; }

		[[nodiscard]] inline float GetPosX() noexcept { return Pos.x; }

		[[nodiscard]] inline float GetPosY() noexcept { return Pos.y; }

	private:
		glm::dvec2 Pos{0};
	};

}