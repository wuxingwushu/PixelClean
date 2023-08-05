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
				if (mPixelAttributeS[x][y].Collision)
				{
					OutlinePointJudge(x,y);
				}
			}
		}
	}

	void ObjectCollision::OutlinePointJudge(int x, int y) {
		//左上角
		if (!PixelAttributeCollision(x - 1, y) || !PixelAttributeCollision(x, y - 1) || !PixelAttributeCollision(x - 1, y - 1)) {
			mOutlinePointSet[OutlinePointSize].x = x;
			mOutlinePointSet[OutlinePointSize].y = y;
			OutlinePointSize++;
		}
		//左下角
		if (!PixelAttributeCollision(x, y + 1)) {
			mOutlinePointSet[OutlinePointSize].x = x;
			mOutlinePointSet[OutlinePointSize].y = y + 1;
			OutlinePointSize++;
		}
		//右上角
		if (!(PixelAttributeCollision(x + 1, y - 1) || PixelAttributeCollision(x + 1, y))) {
			mOutlinePointSet[OutlinePointSize].x = x + 1;
			mOutlinePointSet[OutlinePointSize].y = y;
			OutlinePointSize++;
		}
		//右下角
		if (!(PixelAttributeCollision(x + 1, y) || PixelAttributeCollision(x + 1, y + 1) || PixelAttributeCollision(x, y + 1))) {
			mOutlinePointSet[OutlinePointSize].x = x + 1;
			mOutlinePointSet[OutlinePointSize].y = y + 1;
			OutlinePointSize++;
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

	[[nodiscard]] CollisionInfo ObjectCollision::RelativeCoordinateSystemRadialCollisionDetection(glm::dvec2 Start, glm::dvec2 End) {
		CollisionInfo LCollisionInfo = RadialCollisionDetection(
			vec2angle((glm::vec2(Start) - mPos), -mAngleFloat),
			vec2angle((glm::vec2(End) - mPos), -mAngleFloat)
		);
		/*if (LCollisionInfo.Collision)
		{
			LCollisionInfo.Pos = glm::vec2(vec2angle(LCollisionInfo.Pos, mAngleFloat)) + mPos;
		}*/
		return LCollisionInfo;
	}

	[[nodiscard]] CollisionInfo ObjectCollision::RadialCollisionDetection(glm::ivec2 Start, glm::ivec2 End) {
		int dx = abs(End.x - Start.x);
		int dy = abs(End.y - Start.y);
		int sx = (Start.x < End.x) ? 1 : -1;
		int sy = (Start.y < End.y) ? 1 : -1;
		int err = dx - dy;
		int e2;
		while (true) {
			if (GetFixedCollisionBool(Start)) {
				return { true, Start };
			}
			if (Start.x == End.x && Start.y == End.y) {
				return { false, Start };
			}
			e2 = 2 * err;
			if (e2 > -dy) {
				err -= dy;
				Start.x += sx;
			}
			if (GetFixedCollisionBool(Start)) {
				return { true, Start };
			}
			if (Start.x == End.x && Start.y == End.y) {
				return { false, Start };
			}
			if (e2 < dx) {
				err += dx;
				Start.y += sy;
			}
		}
	}
}
