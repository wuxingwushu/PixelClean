#pragma once
#include "StructuralComponents.h"


namespace SquarePhysics {

	class FixedSizeTerrain
	{
		typedef void (*_TerrainCollisionCallback)(int x, int y, void* mclass);

	public:
		FixedSizeTerrain(
			unsigned int x, 
			unsigned int y, 
			unsigned int SideLength);

		~FixedSizeTerrain();

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


		void SetPosX(float x) { PosX = x; }

		void SetPosY(float y) { PosY = y; }

		void SetOrigin(unsigned int x, unsigned int y) {
			OriginX = x;
			OriginY = y;
		}

		[[nodiscard]] unsigned int GetTerrainX() { return mNumberX; }

		[[nodiscard]] unsigned int GetTerrainY() { return mNumberY; }

		[[nodiscard]] float GetPosX() { return PosX; }

		[[nodiscard]] float GetPosY() { return PosY; }

		[[nodiscard]] PixelAttribute** GetPixelAttributeData(){ return mPixelAttributeS; }

		[[nodiscard]] bool GetFixedCollisionBool(int x, int y) { 
			x += OriginX;
			y += OriginY;
			if (x > mNumberX || x < 0)
			{
				return false;
			}
			if (y > mNumberY || y < 0)
			{
				return false;
			}
			return mPixelAttributeS[x][y].Collision; 
		}

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

		

	private:
		void* mClass = nullptr;
		unsigned int OriginX = 0;
		unsigned int OriginY = 0;
		float PosX = 0;
		float PosY = 0;
		unsigned int mNumberX;
		unsigned int mNumberY;
		unsigned int mSideLength;
		PixelAttribute** mPixelAttributeS;

		
	};

}