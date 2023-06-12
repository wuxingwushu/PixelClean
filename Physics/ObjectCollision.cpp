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
		for (size_t i = 0; i < mNumberX; i++)
		{
			delete mPixelAttributeS[i];
		}
		delete mPixelAttributeS;
	}

	void ObjectCollision::OutlineCalculate() {
		OutlinePointSize = 0;
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
			if (!PixelAttributeCollision(mNumberX - 1, y + 1) && mPixelAttributeS[mNumberX - 1][y].Collision)
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
				if (!PixelAttributeCollision(x - 1, y) || !PixelAttributeCollision(x, y - 1) || !PixelAttributeCollision(x - 1, y - 1)) {
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
			if (!PixelAttributeCollision(x, y + 1)) {
				return true;//没有那就一定在外围
			}
			else {
				return false;//下面那个点，有碰撞，就不需要加了，避免重复
			}
		}
	}


	bool ObjectCollision::PixelCollision(glm::ivec2 dian) {
		dian -= glm::ivec2(mPos);//网格体为中心
		dian = vec2angle(dian, -mAngleFloat);//减除玩家的角度
		if (GetFixedCollisionBool(dian)) {
			SetFixedCollisionBool(dian);
			CollisionCallback(dian.x, dian.y);
			OutlineCalculate();
			return true;
		}
		else {
			return false;
		}
	}
}
