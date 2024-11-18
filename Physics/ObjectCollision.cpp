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
		for (size_t x = 0; x < mNumber.x; ++x)
		{
			for (size_t y = 0; y < mNumber.y; ++y)
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
		float LSideLength = mNumber.x * mNumber.y;
		mBarycenter.x /= LSideLength;
		mBarycenter.y /= LSideLength;
		LSideLength = float(mSideLength) / 2;
		mBarycenter.x += LSideLength;
		mBarycenter.y += LSideLength;
	}

	void ObjectCollision::CalculationBarycenter() {
		mQuality = 0;
		for (size_t x = 0; x < mNumber.x; ++x)
		{
			for (size_t y = 0; y < mNumber.y; ++y)
			{
				if (at({ x,y })->Collision)
				{
					mQuality += 0.01f;
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

	CollisionInfo ObjectCollision::PixelCollision(glm::dvec2 dian) {
		dian -= mPos;//网格体为中心
		dian = vec2angle(dian, { mAngle.x, -mAngle.y });//减除玩家的角度// -mAngleFloat  ->   { mAngle.x, -mAngle.y}
		if (GetFixedCollisionCompensateBool(dian)) {
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
		Start = vec2angle(Start - mPos, { mAngle.x, -mAngle.y} ); // -mAngleFloat  ->   { mAngle.x, -mAngle.y}
		End = vec2angle(End - mPos, { mAngle.x, -mAngle.y });// -mAngleFloat  ->   { mAngle.x, -mAngle.y}
		glm::ivec2 IStart = Start, IEnd = End;
		if (Start.x < 0)--IStart.x;//负值偏移
		if (Start.y < 0)--IStart.y;//负值偏移
		if (End.x < 0)--IEnd.x;//负值偏移
		if (End.y < 0)--IEnd.y;//负值偏移
		CollisionInfo LCollisionInfo = RadialCollisionDetection(IStart, IEnd);
		if (LCollisionInfo.Collision)
		{
			Direction = vec2angle(Direction, { mAngle.x, -mAngle.y });// -mAngleFloat  ->   { mAngle.x, -mAngle.y}

			glm::dvec2 YEnd = End - glm::dvec2(glm::ivec2(End));
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

			if (Depth < 0) {
				Depth = 0;
			}

			End += SquareToRadial(-Depth, 1 + Depth,  -Depth, 1 + Depth, YEnd, Direction);
			LCollisionInfo.Pos = vec2angle(End, mAngle) + mPos;
		}
		return LCollisionInfo;
	}

	[[nodiscard]] CollisionInfo ObjectCollision::RelativeCoordinateSystemRadialCollisionDetection(glm::dvec2 Start, glm::dvec2 End) {
		Start = vec2angle(Start - mPos, { mAngle.x, -mAngle.y });// -mAngleFloat  ->   { mAngle.x, -mAngle.y}
		End = vec2angle(End - mPos, { mAngle.x, -mAngle.y });// -mAngleFloat  ->   { mAngle.x, -mAngle.y}
		if (Start.x < 0)--Start.x;
		if (Start.y < 0)--Start.y;
		if (End.x < 0)--End.x;
		if (End.y < 0)--End.y;
		return RadialCollisionDetection(Start, End);
	}
}
