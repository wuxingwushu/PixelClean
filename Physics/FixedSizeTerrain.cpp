#include "FixedSizeTerrain.h"
#include "../DebugLog.h"

namespace SquarePhysics {

	FixedSizeTerrain::FixedSizeTerrain(
		unsigned int x, 
		unsigned int y, 
		unsigned int SideLength
	) : GridDecorator(x, y, SideLength)
	{
		LOGD("[FixedSizeTerrain] Constructor");
	}

	FixedSizeTerrain::~FixedSizeTerrain()
	{
		LOGD("[FixedSizeTerrain] Destructor");
	}
}