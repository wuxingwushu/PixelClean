#pragma once
#include "StructuralComponents.h"
#include "Callback.h"

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

	bool DestroyModePixel(int x, int y, bool Bool, float Angle, ObjectDecorator* mObject, GridDecorator* mGrid, void* Data);

	bool DestroyModeCross(int x, int y, bool Bool, float Angle, ObjectDecorator* mObject, GridDecorator* mGrid, void* Data);

	bool DestroyModeSquare(int x, int y, bool Bool, float Angle, ObjectDecorator* mObject, GridDecorator* mGrid, void* Data);

	bool DestroyModeSquareDA(int x, int y, bool Bool, float Angle, ObjectDecorator* mObject, GridDecorator* mGrid, void* Data);

}