#pragma once
#include <glm/glm.hpp>
#include "PhysicsCalculate.h"
#include "StructuralComponents.h"
#include "Callback.h"

namespace SquarePhysics {

	class GridDecorator //装饰器模式
	{
	public:
		GridDecorator(unsigned int x, unsigned int y, unsigned int SideLength, bool Structure = true)
			: mNumberX(x), mNumberY(y), mSideLength(SideLength)
		{
			if (Structure) {
				mPixelAttributeS = new PixelAttribute * [mNumberX];
				for (size_t i = 0; i < mNumberX; i++)
				{
					mPixelAttributeS[i] = new PixelAttribute[mNumberY];
				}
			}
		}
		~GridDecorator() {
			if (mPixelAttributeS != nullptr) {
				for (size_t i = 0; i < mNumberX; i++)
				{
					delete mPixelAttributeS[i];
				}
				delete mPixelAttributeS;
			}
		}

		//设置原点
		void SetOrigin(int x, int y) {
			OriginX = x;
			OriginY = y;
		}

		[[nodiscard]] unsigned int GetObjectX() { return mNumberX; }

		[[nodiscard]] unsigned int GetObjectY() { return mNumberY; }





		[[nodiscard]] PixelAttribute** GetPixelAttribute() { return mPixelAttributeS; }


		//设置某个点
		virtual bool SetFixedCollisionBool(glm::ivec2 pos, bool Bool) {
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
			if (mPixelAttributeS[pos.x][pos.y].Collision == Bool) {
				return false;
			}
			mPixelAttributeS[pos.x][pos.y].Collision = Bool;
			return true;
		}

		virtual [[nodiscard]] bool GetFixedCollisionBool(glm::ivec2 pos) {
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
		virtual [[nodiscard]] float GetFrictionCoefficient(glm::ivec2 pos) {
			pos.x += OriginX;
			pos.y += OriginY;
			if (pos.x >= mNumberX || pos.x < 0)
			{
				return 1.0f;
			}
			if (pos.y >= mNumberY || pos.y < 0)
			{
				return 1.0f;
			}
			return mPixelAttributeS[pos.x][pos.y].FrictionCoefficient;
		}

		//判断点是否出界
		virtual [[nodiscard]] bool BoundaryJudge(glm::ivec2 pos) {
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
		virtual [[nodiscard]] bool CrossingTheBoundary(glm::ivec2 pos) {
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


		//设置回调函数的指针和输入
		void SetCollisionCallback(_TerrainCollisionCallback CollisionCallback, void* mclass) {
			mCollisionCallback = CollisionCallback;
			mClass = mclass;
		}



		//调用回调函数
		virtual void CollisionCallback(int x, int y, bool Bool) {
			if (SetFixedCollisionBool({x, y}, Bool)) {
				x += OriginX;
				y += OriginY;
				mCollisionCallback(x, y, Bool, mClass);
			}
		}


		
		//（输入是 Object 的 mPixelAttributeS 数组索引两个点，返回是 mPixelAttributeS 数组索引）
		virtual [[nodiscard]] CollisionInfo RadialCollisionDetection(glm::ivec2 Start, glm::ivec2 End) {
			assert(true && "RadialCollisionDetection Absence Reconfiguration");
			return CollisionInfo{};
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