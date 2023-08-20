#pragma once
#include <glm/glm.hpp>
#include "PhysicsCalculate.h"
#include "StructuralComponents.h"
#include "Callback.h"

namespace SquarePhysics {

	class GridDecorator //装饰器模式
	{
	public:
		GridDecorator(unsigned int x, unsigned int y, unsigned int SideLength)
			: mNumberX(x), mNumberY(y), mSideLength(SideLength)
		{
			mPixelAttributeS = new PixelAttribute * [mNumberX];
			for (size_t i = 0; i < mNumberX; i++)
			{
				mPixelAttributeS[i] = new PixelAttribute[mNumberY];
			}
		}
		~GridDecorator() {}

		//设置原点
		void SetOrigin(int x, int y) {
			OriginX = x;
			OriginY = y;
		}

		[[nodiscard]] unsigned int GetObjectX() { return mNumberX; }

		[[nodiscard]] unsigned int GetObjectY() { return mNumberY; }





		//设置回调函数的指针和输入
		void SetCollisionCallback(_TerrainCollisionCallback CollisionCallback, void* mclass) {
			mCollisionCallback = CollisionCallback;
			mClass = mclass;
		}

		

		//调用回调函数
		void CollisionCallback(int x, int y, bool Bool) {
			x += OriginX;
			y += OriginY;
			mCollisionCallback(x, y, Bool, mClass);
		}







		[[nodiscard]] PixelAttribute** GetPixelAttribute() { return mPixelAttributeS; }


		//破坏某个点
		void SetFixedCollisionBool(glm::ivec2 pos) {
			pos.x += OriginX;
			pos.y += OriginY;
			if (pos.x >= mNumberX || pos.x < 0)
			{
				return;
			}
			if (pos.y >= mNumberY || pos.y < 0)
			{
				return;
			}
			mPixelAttributeS[pos.x][pos.y].Collision = false;
		}

		[[nodiscard]] bool GetFixedCollisionBool(glm::ivec2 pos) {
			pos.x += OriginX;
			pos.y += OriginY;
			if (pos.x >= mNumberX || pos.x < 0)
			{
				return false;
			}
			if (pos.y >= mNumberY || pos.y < 0)
			{
				return false;
			}
			return mPixelAttributeS[pos.x][pos.y].Collision;
		}

		//获取某个像素带点的摩檫力系数
		[[nodiscard]] float GetFrictionCoefficient(glm::ivec2 pos) {
			pos.x += OriginX;
			pos.y += OriginY;
			if (pos.x > mNumberX || pos.x < 0)
			{
				return 1.0f;
			}
			if (pos.y > mNumberY || pos.y < 0)
			{
				return 1.0f;
			}
			return mPixelAttributeS[pos.x][pos.y].FrictionCoefficient;
		}

		//判断点是否出界
		[[nodiscard]] bool BoundaryJudge(glm::ivec2 pos) {
			if (pos.x >= mNumberX && pos.x < 0)
			{
				return false;
			}
			if (pos.y >= mNumberY && pos.y < 0)
			{
				return false;
			}
			return true;
		}

		//判断点是否出界（中心偏移）
		[[nodiscard]] bool CrossingTheBoundary(glm::ivec2 pos) {
			pos.x += OriginX;
			if ((pos.x >= mNumberX) || (pos.x < 0))
			{
				return true;
			}
			pos.y += OriginY;
			if ((pos.y >= mNumberY) || (pos.y < 0))
			{
				return true;
			}
			return false;
		}




		//像素边长
		unsigned int mSideLength = 1;
		//原点
		int OriginX = 0;
		int OriginY = 0;
		//大小
		unsigned int mNumberX = 0;
		unsigned int mNumberY = 0;
		PixelAttribute** mPixelAttributeS{ nullptr };//点数据

		//回调函数指针
		_TerrainCollisionCallback mCollisionCallback = nullptr;
		void* mClass = nullptr;//回调数据,模型
	};

}