#include "PixelCollision.h"


namespace SquarePhysics {
	
	PixelCollision::PixelCollision(unsigned int SideLength) :
		mSideLength(SideLength)
	{
		float LSideLength = float(mSideLength) / 2;
		mBarycenter = { LSideLength, LSideLength };
	}

	PixelCollision::~PixelCollision()
	{
	}

}