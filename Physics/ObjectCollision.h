#pragma once
#include "StructuralComponents.h"
#include "ObjectDecorator.h"


namespace SquarePhysics {

	class ObjectCollision :public ObjectDecorator
	{
		typedef void (*_TerrainCollisionCallback)(int x, int y, void* mclass);

	public:
		ObjectCollision(
			unsigned int x,
			unsigned int y,
			unsigned int SideLength);

		~ObjectCollision();

		void SetCollisionCallback(_TerrainCollisionCallback CollisionCallback, void* mclass) {
			mCollisionCallback = CollisionCallback;
			mClass = mclass;
		}

		_TerrainCollisionCallback mCollisionCallback = nullptr;

		void CollisionCallback(int x, int y) {
			x += OriginX;
			y += OriginY;
			mCollisionCallback(x, y, mClass);
		}

		void SetOrigin(int x, int y) {
			OriginX = x;
			OriginY = y;
		}

		[[nodiscard]] unsigned int GetObjectX() { return mNumberX; }

		[[nodiscard]] unsigned int GetObjectY() { return mNumberY; }

		[[nodiscard]] PixelAttribute** GetPixelAttribute() { return mPixelAttributeS; }



		void SetFixedCollisionBool(int x, int y) {
			x += OriginX;
			y += OriginY;
			if (x > mNumberX || x < 0)
			{
				return;
			}
			if (y > mNumberY || y < 0)
			{
				return;
			}
			mPixelAttributeS[x][y].Collision = false;
		}

		[[nodiscard]] float GetFixedFrictionCoefficient(int x, int y) {
			x += OriginX;
			y += OriginY;
			if (x > mNumberX || x < 0)
			{
				return 1.0f;
			}
			if (y > mNumberY || y < 0)
			{
				return 1.0f;
			}
			return mPixelAttributeS[x][y].FrictionCoefficient;
		}

		[[nodiscard]] bool BoundaryJudge(int x, int y) {
			if (x < mNumberX)
			{
				return false;
			}
			if (y < mNumberY)
			{
				return false;
			}
			return true;
		}

		[[nodiscard]] bool CrossingTheBoundary(int x, int y) {
			int Oxy = -int(OriginX);
			int Nxy = (mNumberX - OriginX);
			if ((x >= Nxy) || (x < Oxy))
			{
				return true;
			}
			Oxy = -int(OriginY);
			Nxy = (mNumberY - OriginY);
			if ((y >= Nxy) || (y < Oxy))
			{
				return true;
			}
			return false;
		}





		void OutlineCalculate();

		bool OutlinePointJudge(int x, int y, bool UpAndDown);

		bool UpPixelAttributeCollision(int x, int y) {
			if ((x < 0) || (x >= mNumberX) || (y < 0) || (y >= mNumberY))
			{
				return false;
			}
			return mPixelAttributeS[x][y].Collision;
		}

		bool DownPixelAttributeCollision(int x, int y) {
			if ((x < 0) || (x >= mNumberX) || (y < 0) || (y >= mNumberY))
			{
				return false;
			}
			return mPixelAttributeS[x][y].Collision;
		}

		unsigned int GetOutlinePointSize() { return OutlinePointSize; }
		glm::vec2 GetOutlinePointSet(unsigned int i) { return { mOutlinePointSet[i].x - OriginX, mOutlinePointSet[i].y - OriginY }; }


	private:
		void* mClass = nullptr;
		int OriginX = 0;
		int OriginY = 0;
		unsigned int mNumberX;
		unsigned int mNumberY;
		unsigned int mSideLength;
		PixelAttribute** mPixelAttributeS;

		unsigned int OutlinePointSize = 0;
		glm::vec2 mOutlinePointSet[256];
	};

}
