#include "FixedSizeTerrain.h"

namespace SquarePhysics {

	FixedSizeTerrain::FixedSizeTerrain(
		unsigned int x, 
		unsigned int y, 
		unsigned int SideLength
	) :
		mNumberX(x),
		mNumberY(y),
		mSideLength(SideLength)
	{
		mPixelAttributeS = new PixelAttribute * [mNumberX];
		for (size_t i = 0; i < mNumberX; i++)
		{
			mPixelAttributeS[i] = new PixelAttribute[mNumberY];
		}
	}

	FixedSizeTerrain::~FixedSizeTerrain()
	{

	}

}