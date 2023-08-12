#include "DestroyMode.h"
#include "ObjectCollision.h"
#include "FixedSizeTerrain.h"
#include <map>

namespace SquarePhysics {

	std::map<DestroyModeEnum, _DestroyModeCallback> DestroyModeMap{
		{ DestroyModeEnum::DestroyModePixel, DestroyModePixel },
		{ DestroyModeEnum::DestroyModeCross, DestroyModeCross },
		{ DestroyModeEnum::DestroyModeSquare, DestroyModeSquare },
	};

	_DestroyModeCallback GetDestroyMode(DestroyModeEnum Enum) {
		if (DestroyModeMap.find(Enum) == DestroyModeMap.end()) {
			return DestroyModePixel;
		}
		else {
			return DestroyModeMap[Enum];
		}
	}

	void DestroyModePixel(int x, int y, void* mclass, bool Bool) {
		GridDecorator* Class;
		if (Bool)
		{
			Class = (FixedSizeTerrain*)mclass;
		}
		else {
			Class = (ObjectCollision*)mclass;
		}

		Class->CollisionCallback(x, y, false);
		Class->SetFixedCollisionBool({ x, y });
	}

	void DestroyModeCross(int x, int y, void* mclass, bool Bool) {
		GridDecorator* Class;
		if (Bool)
		{
			Class = (FixedSizeTerrain*)mclass;
		}
		else {
			Class = (ObjectCollision*)mclass;
		}

		Class->CollisionCallback(x, y, false);
		Class->SetFixedCollisionBool({ x, y });
		Class->CollisionCallback(x + 1, y, false);
		Class->SetFixedCollisionBool({ x + 1, y });
		Class->CollisionCallback(x, y + 1, false);
		Class->SetFixedCollisionBool({ x, y + 1 });
		Class->CollisionCallback(x - 1, y, false);
		Class->SetFixedCollisionBool({ x - 1, y });
		Class->CollisionCallback(x, y - 1, false);
		Class->SetFixedCollisionBool({ x, y - 1 });
	}

	void DestroyModeSquare(int x, int y, void* mclass, bool Bool) {
		GridDecorator* Class;
		if (Bool)
		{
			Class = (FixedSizeTerrain*)mclass;
		}
		else {
			Class = (ObjectCollision*)mclass;
		}

		Class->CollisionCallback(x, y, false);
		Class->SetFixedCollisionBool({ x, y });

		Class->CollisionCallback(x + 1, y+1, false);
		Class->SetFixedCollisionBool({ x + 1, y+1 });
		Class->CollisionCallback(x + 1, y, false);
		Class->SetFixedCollisionBool({ x + 1, y });
		Class->CollisionCallback(x + 1, y-1, false);
		Class->SetFixedCollisionBool({ x + 1, y-1 });

		Class->CollisionCallback(x, y + 1, false);
		Class->SetFixedCollisionBool({ x, y + 1 });
		Class->CollisionCallback(x, y, false);
		Class->SetFixedCollisionBool({ x, y });
		Class->CollisionCallback(x, y - 1, false);
		Class->SetFixedCollisionBool({ x, y - 1 });

		Class->CollisionCallback(x - 1, y+1, false);
		Class->SetFixedCollisionBool({ x - 1, y+1 });
		Class->CollisionCallback(x - 1, y, false);
		Class->SetFixedCollisionBool({ x - 1, y });
		Class->CollisionCallback(x - 1, y-1, false);
		Class->SetFixedCollisionBool({ x - 1, y-1 });
	}

}