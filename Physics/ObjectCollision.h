#pragma once
#include "StructuralComponents.h"
#include "ObjectDecorator.h"


namespace SquarePhysics {

	class ObjectCollision :public ObjectDecorator
	{
		//回调函数的类型
		typedef void (*_TerrainCollisionCallback)(int x, int y, void* mclass);

	public:
		ObjectCollision(
			unsigned int x,
			unsigned int y,
			unsigned int SideLength);

		~ObjectCollision();

		//设置回调函数的指针和输入
		void SetCollisionCallback(_TerrainCollisionCallback CollisionCallback, void* mclass) {
			mCollisionCallback = CollisionCallback;
			mClass = mclass;
		}

		//回调函数指针
		_TerrainCollisionCallback mCollisionCallback = nullptr;

		//调用回调函数
		void CollisionCallback(int x, int y) {
			x += OriginX;
			y += OriginY;
			mCollisionCallback(x, y, mClass);
		}

		//设置原点
		void SetOrigin(int x, int y) {
			OriginX = x;
			OriginY = y;
		}

		[[nodiscard]] unsigned int GetObjectX() { return mNumberX; }

		[[nodiscard]] unsigned int GetObjectY() { return mNumberY; }

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

		bool GetFixedCollisionBool(glm::ivec2 pos) {
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
		
		//判断点是否出界
		[[nodiscard]] bool BoundaryJudge(int x, int y) {
			if (x >= mNumberX && x < 0)
			{
				return false;
			}
			if (y >= mNumberY && y < 0)
			{
				return false;
			}
			return true;
		}

		//判断点是否出界（中心偏移）
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


		/*++++++++++++++++++++++++++++++++       碰撞有关       ++++++++++++++++++++++++++++++++*/

		//计算当前碰撞体的 外骨架
		void OutlineCalculate();

		//判断点是否符合骨架的条件
		void OutlinePointJudge(int x, int y);
		
		//获取点信息
		bool PixelAttributeCollision(int x, int y) {
			if ((x < 0) || (x >= mNumberX) || (y < 0) || (y >= mNumberY))
			{
				return false;
			}
			return mPixelAttributeS[x][y].Collision;
		}

		//有多少个外骨架点
		unsigned int GetOutlinePointSize() { return OutlinePointSize; }
		//获取第 I 个外骨架点
		glm::vec2 GetOutlinePointSet(unsigned int i) { return { mOutlinePointSet[i].x - OriginX, mOutlinePointSet[i].y - OriginY }; }


		//子弹对玩家的碰撞判断
		bool PixelCollision(glm::ivec2 dian);
		//路径碰撞判断
		[[nodiscard]] CollisionInfo RelativeCoordinateSystemRadialCollisionDetection(glm::dvec2 Start, glm::dvec2 End);
		[[nodiscard]] CollisionInfo RadialCollisionDetection(glm::ivec2 Start, glm::ivec2 End);


	private:
		void* mClass = nullptr;//回调数据
		//原点
		int OriginX = 0;
		int OriginY = 0;
		//地图大小
		unsigned int mNumberX;
		unsigned int mNumberY;
		unsigned int mSideLength;//边长
		PixelAttribute** mPixelAttributeS;//点数据

		unsigned int OutlinePointSize = 0;//点集的数量
		glm::vec2 mOutlinePointSet[256];//外包裹点集
	};

}
