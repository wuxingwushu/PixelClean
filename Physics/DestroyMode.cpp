#include "DestroyMode.h"
#include "ObjectCollision.h"
#include "FixedSizeTerrain.h"
#include <map>

namespace SquarePhysics {

	std::map<DestroyModeEnum, _DestroyModeCallback> DestroyModeMap{
		{ DestroyModeEnum::DestroyModePixel, DestroyModePixel },
		{ DestroyModeEnum::DestroyModeCross, DestroyModeCross },
		{ DestroyModeEnum::DestroyModeSquare, DestroyModeSquare },
		{ DestroyModeEnum::DestroyModeSquareDA, DestroyModeSquareDA },
	};

	_DestroyModeCallback GetDestroyMode(DestroyModeEnum Enum) {
		if (DestroyModeMap.find(Enum) == DestroyModeMap.end()) {
			return DestroyModePixel;
		}
		else {
			return DestroyModeMap[Enum];
		}
	}

	bool DestroyModePixel(int x, int y, bool Bool, float Angle, ObjectDecorator* mObject, GridDecorator* mGrid, void* Data) {
		std::cout << Angle << std::endl;
		mGrid->CollisionCallback(x, y, false, mObject);

		return true;
	}

	bool DestroyModeCross(int x, int y, bool Bool, float Angle, ObjectDecorator* mObject, GridDecorator* mGrid, void* Data) {
		mGrid->CollisionCallback(x, y, false, mObject);
		mGrid->CollisionCallback(x + 1, y, false, mObject);
		mGrid->CollisionCallback(x, y + 1, false, mObject);
		mGrid->CollisionCallback(x - 1, y, false, mObject);
		mGrid->CollisionCallback(x, y - 1, false, mObject);

		return true;
	}

	bool DestroyModeSquare(int x, int y, bool Bool, float Angle, ObjectDecorator* mObject, GridDecorator* mGrid, void* Data) {
		const int Range = 1;
		for (int i = -Range; i <= Range; ++i) {
			for (int j = -Range; j <= Range; ++j) {
				mGrid->CollisionCallback(x + i, y + j, false, mObject);
			}
		}
		return true;
	}

	bool DestroyModeSquareDA(int x, int y, bool Bool, float Angle, ObjectDecorator* mObject, GridDecorator* mGrid, void* Data) {
		const int Range = 5;
		for (int i = -Range; i <= Range; ++i) {
			for (int j = -Range; j <= Range; ++j) {
				mGrid->CollisionCallback(x + i, y + j, false, mObject);
			}
		}
		return true;
	}

}