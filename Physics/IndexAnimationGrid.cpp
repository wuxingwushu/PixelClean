#include "IndexAnimationGrid.h"


namespace SquarePhysics {

	

	IndexAnimationGrid::IndexAnimationGrid(unsigned int x, unsigned int y, unsigned int SideLength, unsigned int Frames)
		:ObjectCollision(x, y, SideLength), mFrames(Frames)
	{
		OutlinePointSizeS = new unsigned int[Frames];
		mOutlinePointSet = new glm::vec2*[Frames];
		for (size_t i = 0; i < Frames; i++)
		{
			mOutlinePointSet[i] = new glm::vec2[x * y * 2];
		}
	}

	IndexAnimationGrid::~IndexAnimationGrid()
	{
		delete OutlinePointSizeS;
		for (size_t i = 0; i < mFrames; i++)
		{
			delete mOutlinePointSet[i];
		}
		delete mOutlinePointSet;
	}

	//计算当前碰撞体的 外骨架
	void IndexAnimationGrid::OutlineCalculate() {
		int LSCurrentFrame = CurrentFrame;
		for (size_t i = 0; i < mFrames; i++)
		{
			SetCurrentFrame(i);
			OutlinePointSizeS[CurrentFrame] = 0;
			mQuality = 0;
			for (size_t x = 0; x < mNumber.x; ++x)
			{
				for (size_t y = 0; y < mNumber.y; ++y)
				{
					if (at({ x,y })->Collision)
					{
						mQuality += 0.01f;
						OutlinePointJudge(x, y);
						mBarycenter.x += x;
						mBarycenter.y += y;
					}
				}
			}
			float LSideLength = mNumber.x * mNumber.y;
			mBarycenter.x /= LSideLength;
			mBarycenter.y /= LSideLength;
			LSideLength = float(mSideLength) / 2;
			mBarycenter.x += LSideLength;
			mBarycenter.y += LSideLength;
		}
		CurrentFrame = LSCurrentFrame;
	}

	//判断点是否符合骨架的条件
	inline void IndexAnimationGrid::OutlinePointJudge(int x, int y) {
		//左上角
		if (!PixelAttributeCollision(x - 1, y) || !PixelAttributeCollision(x, y - 1) || !PixelAttributeCollision(x - 1, y - 1)) {
			mOutlinePointSet[CurrentFrame][OutlinePointSizeS[CurrentFrame]].x = x;
			mOutlinePointSet[CurrentFrame][OutlinePointSizeS[CurrentFrame]].y = y;
			++OutlinePointSizeS[CurrentFrame];
		}
		//左下角
		if (!PixelAttributeCollision(x, y + 1)) {
			mOutlinePointSet[CurrentFrame][OutlinePointSizeS[CurrentFrame]].x = x;
			mOutlinePointSet[CurrentFrame][OutlinePointSizeS[CurrentFrame]].y = y + 1;
			++OutlinePointSizeS[CurrentFrame];
		}
		//右上角
		if (!(PixelAttributeCollision(x + 1, y - 1) || PixelAttributeCollision(x + 1, y))) {
			mOutlinePointSet[CurrentFrame][OutlinePointSizeS[CurrentFrame]].x = x + 1;
			mOutlinePointSet[CurrentFrame][OutlinePointSizeS[CurrentFrame]].y = y;
			++OutlinePointSizeS[CurrentFrame];
		}
		//右下角
		if (!(PixelAttributeCollision(x + 1, y) || PixelAttributeCollision(x + 1, y + 1) || PixelAttributeCollision(x, y + 1))) {
			mOutlinePointSet[CurrentFrame][OutlinePointSizeS[CurrentFrame]].x = x + 1;
			mOutlinePointSet[CurrentFrame][OutlinePointSizeS[CurrentFrame]].y = y + 1;
			++OutlinePointSizeS[CurrentFrame];
		}
	}

}