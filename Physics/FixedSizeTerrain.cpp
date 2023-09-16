#include "FixedSizeTerrain.h"

namespace SquarePhysics {

	FixedSizeTerrain::FixedSizeTerrain(
		unsigned int x, 
		unsigned int y, 
		unsigned int SideLength
	) : GridDecorator(x, y, SideLength)
	{
	}

	FixedSizeTerrain::~FixedSizeTerrain()
	{
	}
}