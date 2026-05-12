#include "PixelCollision.h"
#include "../DebugLog.h"

namespace SquarePhysics {
	
	PixelCollision::PixelCollision(unsigned int SideLength) :
		mSideLength(SideLength)
	{
		LOGD("[PixelCollision] Constructor");
		float LSideLength = float(mSideLength) / 2;
		mBarycenter = { LSideLength, LSideLength };
	}

	PixelCollision::~PixelCollision()
	{
		LOGD("[PixelCollision] Destructor");
	}

}