#pragma once
#include "StructuralComponents.h"

namespace SquarePhysics {

	enum class DestroyModeEnum
	{
		DestroyModePixel = 0,
		DestroyModeSquare,
		DestroyModeSquareDA,
		DestroyModeCross//这个作为底，可以得知有多少个 Enum
	};

	constexpr int DestroyModeEnumNumber = static_cast<int>(DestroyModeEnum::DestroyModeCross);

	_DestroyModeCallback GetDestroyMode(DestroyModeEnum Enum);

	void DestroyModePixel(int x, int y, void* mclass, bool Bool);

	void DestroyModeCross(int x, int y, void* mclass, bool Bool);

	void DestroyModeSquare(int x, int y, void* mclass, bool Bool);

	void DestroyModeSquareDA(int x, int y, void* mclass, bool Bool);

}