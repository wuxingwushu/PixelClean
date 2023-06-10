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
		mPixelAttributeS = new PixelAttribute * [mNumberX];
		for (size_t i = 0; i < mNumberX; i++)
		{
			mPixelAttributeS[i] = new PixelAttribute[mNumberY];
		}
	}

	ObjectCollision::~ObjectCollision()
	{
	}

	void ObjectCollision::OutlineCalculate() {
		for (size_t x = 0; x < mNumberX; x++)
		{
			for (size_t y = 0; y < mNumberY; y++)
			{
				if (OutlinePointJudge(x,y,1))
				{
					mOutlinePointSet[OutlinePointSize].x = x;
					mOutlinePointSet[OutlinePointSize].y = y;
					OutlinePointSize++;
				}
				if (OutlinePointJudge(x, y, 0) && mPixelAttributeS[x][y].Collision)
				{
					mOutlinePointSet[OutlinePointSize].x = x;
					mOutlinePointSet[OutlinePointSize].y = y + 1;
					OutlinePointSize++;
				}
			}
		}

		for (size_t y = 0; y < mNumberY; y++)
		{
			if (mPixelAttributeS[mNumberX - 1][y].Collision) {
				mOutlinePointSet[OutlinePointSize].x = mNumberX;
				mOutlinePointSet[OutlinePointSize].y = y;
				OutlinePointSize++;
			}
			if (!DownPixelAttributeCollision(mNumberX - 1, y + 1) && mPixelAttributeS[mNumberX - 1][y].Collision)
			{
				mOutlinePointSet[OutlinePointSize].x = mNumberX;
				mOutlinePointSet[OutlinePointSize].y = y + 1;
				OutlinePointSize++;
			}
		}
	}

	bool ObjectCollision::OutlinePointJudge(int x, int y, bool UpAndDown) {
		if (UpAndDown)
		{
			if (mPixelAttributeS[x][y].Collision)
			{
				if (!UpPixelAttributeCollision(x - 1, y) || !UpPixelAttributeCollision(x, y - 1) || !UpPixelAttributeCollision(x - 1, y - 1)) {
					return true;//在外围
				}
				else {
					return false;//有碰撞，但是在内部，没必要检测
				}
			}
			else {
				return false;//没有碰撞，
			}
		}
		else {
			if (!DownPixelAttributeCollision(x, y + 1)) {
				return true;//没有那就一定在外围
			}
			else {
				return false;//下面那个点，有碰撞，就不需要加了，避免重复
			}
		}
	}
}
