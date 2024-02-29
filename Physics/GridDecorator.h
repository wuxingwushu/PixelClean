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
			: mNumber{ x, y }, mSideLength(SideLength)
		{
			if (Structure) {
				Radius = Modulus(mNumber);
				mPixelAttributeS = new PixelAttribute[mNumber.x * mNumber.y];
			}
		}
		virtual ~GridDecorator() {
			if (mPixelAttributeS != nullptr) {
				delete mPixelAttributeS;
			}
		}

		virtual inline PixelAttribute* at(glm::uvec2 pos) noexcept {
			return &mPixelAttributeS[pos.x * mNumber.y + pos.y];
		}

		//设置原点
		inline void SetOrigin(int x, int y) noexcept { 
			Origin = { x, y };
			glm::ivec2 S = glm::ivec2(mNumber) - Origin;
			S.x = fabs(Origin.x) > fabs(S.x) ? fabs(Origin.x) : fabs(S.x);
			S.y = fabs(Origin.y) > fabs(S.y) ? fabs(Origin.y) : fabs(S.y);
			Radius = Modulus(S);
		}

		[[nodiscard]] inline constexpr glm::uvec2 GetObject() noexcept { return mNumber; }





		//[[nodiscard]] PixelAttribute** GetPixelAttribute() { return mPixelAttributeS; }


		//设置某个点
		virtual inline bool SetFixedCollisionBool(glm::ivec2 pos, bool Bool) noexcept {
			pos.x += Origin.x;
			if (pos.x >= mNumber.x || pos.x < 0)
			{
				return false;
			}
			pos.y += Origin.y;
			if (pos.y >= mNumber.y || pos.y < 0)
			{
				return false;
			}
			if (at(pos)->Collision == Bool) {
				return false;
			}
			at(pos)->Collision = Bool;
			return true;
		}

		//获取网格是否有碰撞（中心原点补偿）
		virtual inline bool GetFixedCollisionCompensateBool(glm::ivec2 pos) {
			pos.x += Origin.x;
			if (pos.x >= mNumber.x || pos.x < 0)
			{
				return false;
			}
			pos.y += Origin.y;
			if (pos.y >= mNumber.y || pos.y < 0)
			{
				return false;
			}
			return at(pos)->Collision;
		}

		//获取网格是否有碰撞
		virtual inline bool GetFixedCollisionBool(glm::uvec2 pos) {
			if ((pos.x >= mNumber.x) || (pos.y >= mNumber.y))
			{
				return false;
			}
			return at(pos)->Collision;
		}

		//获取某个像素带点的摩檫力系数
		virtual inline float GetFrictionCoefficient(glm::ivec2 pos) {
			pos.x += Origin.x;
			if (pos.x >= mNumber.x || pos.x < 0)
			{
				return 1.0f;
			}
			pos.y += Origin.y;
			if (pos.y >= mNumber.y || pos.y < 0)
			{
				return 1.0f;
			}
			return at(pos)->FrictionCoefficient;
		}

		//判断点是否出界
		virtual inline bool BoundaryJudge(glm::uvec2 pos) noexcept {
			if (pos.x >= mNumber.x)
			{
				return false;
			}
			if (pos.y >= mNumber.y)
			{
				return false;
			}
			return true;
		}

		//判断点是否出界（中心偏移）
		virtual inline bool CrossingTheBoundary(glm::ivec2 pos) noexcept {
			pos.x += Origin.x;
			if ((pos.x >= mNumber.x) || (pos.x < 0))
			{
				return true;
			}
			pos.y += Origin.y;
			if ((pos.y >= mNumber.y) || (pos.y < 0))
			{
				return true;
			}
			return false;
		}


		//设置回调函数的指针和输入
		inline void SetCollisionCallback(_TerrainCollisionCallback CollisionCallback, void* mclass) noexcept {
			mCollisionCallback = CollisionCallback;
			mClass = mclass;
		}



		//调用回调函数
		virtual inline void CollisionCallback(int x, int y, bool Bool, ObjectDecorator* object) {
			if (SetFixedCollisionBool({x, y}, Bool)) {
				x += Origin.x;
				y += Origin.y;
				mCollisionCallback(x, y, Bool, object, mClass);
			}
		}


		
		//（输入是 Object 的 mPixelAttributeS 数组索引两个点，返回是 mPixelAttributeS 数组索引）
		virtual [[nodiscard]] CollisionInfo RadialCollisionDetection(glm::ivec2 Start, glm::ivec2 End) {
			Start += Origin;
			End += Origin;
			int dx = abs(End.x - Start.x);
			int dy = abs(End.y - Start.y);
			int sx = (Start.x < End.x) ? 1 : -1;
			int sy = (Start.y < End.y) ? 1 : -1;
			int err = dx - dy;
			int e2;
			while (true) {
				if (Start.x == End.x && Start.y == End.y) {
					return { false, Start - Origin, 0 };
				}
				e2 = 2 * err;
				if (e2 > -dy) {
					err -= dy;
					Start.x += sx;
					if (GetFixedCollisionBool(Start)) {
						return { true, Start - Origin, 0 + (sx == 1 ? 3.14f : 0) };
					}
				}
				if (e2 < dx) {
					err += dx;
					Start.y += sy;
					if (GetFixedCollisionBool(Start)) {
						return { true, Start - Origin, 1.57f - (sy == 1 ? 3.14f : 0) };
					}
				}
			}
		}

		//像素边长
		unsigned int mSideLength = 1;
		//原点
		glm::ivec2 Origin{ 0 };
		//大小
		glm::uvec2 mNumber{ 0 };
		//碰撞最大半径
		float Radius = 0;
		//点数据
		PixelAttribute* mPixelAttributeS{ nullptr };

		//回调函数指针
		_TerrainCollisionCallback mCollisionCallback = nullptr;
		void* mClass = nullptr;//回调数据,模型


		//有多少个外骨架点
		virtual inline unsigned int GetOutlinePointSize() noexcept { return 0; }
		//获取第 I 个外骨架点
		virtual inline glm::vec2 GetOutlinePointSet(unsigned int i) noexcept { return glm::vec2(0); }
	};

}