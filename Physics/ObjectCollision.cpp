#include "ObjectCollision.h"



namespace SquarePhysics {

	ObjectCollision::ObjectCollision(
		unsigned int x,
		unsigned int y,
		unsigned int SideLength
	): GridDecorator(x, y, SideLength)
	{
	}

	ObjectCollision::~ObjectCollision()
	{
	}

	void ObjectCollision::OutlineCalculate() {
		OutlinePointSize = 0;
		mQuality = 0;
		for (size_t x = 0; x < mNumberX; ++x)
		{
			for (size_t y = 0; y < mNumberY; ++y)
			{
				if (at({x,y})->Collision)
				{
					mQuality += 0.01f;
					OutlinePointJudge(x,y);
					mBarycenter.x += x;
					mBarycenter.y += y;
				}
			}
		}
		float LSideLength = mNumberX * mNumberY;
		mBarycenter.x /= LSideLength;
		mBarycenter.y /= LSideLength;
		LSideLength = float(mSideLength) / 2;
		mBarycenter.x += LSideLength;
		mBarycenter.y += LSideLength;
	}

	void ObjectCollision::CalculationBarycenter() {
		mQuality = 0;
		for (size_t x = 0; x < mNumberX; ++x)
		{
			for (size_t y = 0; y < mNumberY; ++y)
			{
				if (at({ x,y })->Collision)
				{
					mQuality += 0.01f;
					mBarycenter.x += x;
					mBarycenter.y += y;
				}
			}
		}
		float LSideLength = mNumberX * mNumberY;
		mBarycenter.x /= LSideLength;
		mBarycenter.y /= LSideLength;
		LSideLength = float(mSideLength) / 2;
		mBarycenter.x += LSideLength;
		mBarycenter.y += LSideLength;
	}

	inline void ObjectCollision::OutlinePointJudge(int x, int y) {
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

	CollisionInfo ObjectCollision::PixelCollision(glm::vec2 dian) {
		dian -= mPos;//网格体为中心
		dian = vec2angle(dian, { mAngle.x, -mAngle.y });//减除玩家的角度// -mAngleFloat  ->   { mAngle.x, -mAngle.y}
		if (GetFixedCollisionBool(dian)) {
			SetFixedCollisionBool(dian, false);
			CollisionCallback(dian.x, dian.y, false, this);
			OutlineCalculate();
			return { true, glm::ivec2(dian) };
		}
		else {
			return { false };
		}
	}

	[[nodiscard]] CollisionInfo ObjectCollision::SquarePhysicsCoordinateSystemRadialCollisionDetection(glm::dvec2 Start, glm::dvec2 End, glm::vec2 Direction) {
		Start = vec2angle((glm::vec2(Start) - mPos), { mAngle.x, -mAngle.y} ); // -mAngleFloat  ->   { mAngle.x, -mAngle.y}
		End = vec2angle((glm::vec2(End) - mPos), { mAngle.x, -mAngle.y });// -mAngleFloat  ->   { mAngle.x, -mAngle.y}
		glm::ivec2 IStart = Start, IEnd = End;
		if (IStart.x < 0)IStart.x -= 1;//负值偏移
		if (IStart.y < 0)IStart.y -= 1;//负值偏移
		if (IEnd.x < 0)IEnd.x -= 1;//负值偏移
		if (IEnd.y < 0)IEnd.y -= 1;//负值偏移
		CollisionInfo LCollisionInfo = RadialCollisionDetection(IStart, IEnd);
		if (LCollisionInfo.Collision)
		{
			Direction = vec2angle(Direction, { mAngle.x, -mAngle.y });// -mAngleFloat  ->   { mAngle.x, -mAngle.y}

			glm::ivec2 IEnd = End;
			glm::dvec2 YEnd = End - glm::dvec2(IEnd);
			if (YEnd.x < 0) {
				YEnd.x += 1;
			}
			if (YEnd.y < 0) {
				YEnd.y += 1;
			}

			int Depth = 0;
			IEnd -= glm::ivec2(LCollisionInfo.Pos);
			if (fabs(IEnd.x) < fabs(IEnd.y)) {
				Depth = fabs(IEnd.y) - 1;
			}
			else
			{
				Depth = fabs(IEnd.x) - 1;
			}

			End += SquareToRadial(-Depth, 1 + Depth,  -Depth, 1 + Depth, YEnd, Direction);
			LCollisionInfo.Pos = glm::vec2(vec2angle(End, mAngle)) + mPos;
		}
		return LCollisionInfo;
	}

	[[nodiscard]] CollisionInfo ObjectCollision::RelativeCoordinateSystemRadialCollisionDetection(glm::dvec2 Start, glm::dvec2 End) {
		CollisionInfo LCollisionInfo = RadialCollisionDetection(
			vec2angle((glm::vec2(Start) - mPos), { mAngle.x, -mAngle.y }),// -mAngleFloat  ->   { mAngle.x, -mAngle.y}
			vec2angle((glm::vec2(End) - mPos), { mAngle.x, -mAngle.y })// -mAngleFloat  ->   { mAngle.x, -mAngle.y}
		);
		return LCollisionInfo;
	}
}
