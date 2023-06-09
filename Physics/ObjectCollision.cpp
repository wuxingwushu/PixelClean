#include "ObjectCollision.h"



namespace SquarePhysics {

	ObjectCollision::ObjectCollision(
		unsigned int x,
		unsigned int y,
		unsigned int SideLength
	) :
		mNumberX(x),
		mNumberY(y),
		mSideLength(SideLength)
	{
		mPixelAttribute = new PixelAttribute * [mNumberX];
		for (size_t i = 0; i < mNumberX; i++)
		{
			mPixelAttribute[i] = new PixelAttribute[mNumberY];
		}
	}

	ObjectCollision::~ObjectCollision()
	{
	}

}
