#pragma once
#include "StructuralComponents.h"
#include "ObjectDecorator.h"
#include "GridDecorator.h"

namespace SquarePhysics {

	class ObjectCollision :public ObjectDecorator, public GridDecorator
	{
		

	public:
		ObjectCollision(
			unsigned int x,
			unsigned int y,
			unsigned int SideLength);

		~ObjectCollision();

		

		

		


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

		//计算重心
		void CalculationBarycenter();

		//有多少个外骨架点
		unsigned int GetOutlinePointSize() { return OutlinePointSize; }
		//获取第 I 个外骨架点
		glm::vec2 GetOutlinePointSet(unsigned int i) { return { mOutlinePointSet[i].x - OriginX, mOutlinePointSet[i].y - OriginY }; }


		//点对玩家的碰撞判断
		CollisionInfo PixelCollision(glm::vec2 dian);
		//路径碰撞判断
		//（输入是 SquarePhysics 坐标的两个点，返回是 SquarePhysics 坐标点， 偏移方向）
		[[nodiscard]] CollisionInfo SquarePhysicsCoordinateSystemRadialCollisionDetection(glm::dvec2 Start, glm::dvec2 End, glm::vec2 Direction);
		//（输入是 SquarePhysics 坐标的两个点，返回是 mPixelAttributeS 数组索引）
		[[nodiscard]] CollisionInfo RelativeCoordinateSystemRadialCollisionDetection(glm::dvec2 Start, glm::dvec2 End);

		

	private:
		unsigned int OutlinePointSize = 0;//点集的数量
		glm::vec2 mOutlinePointSet[256]{};//外包裹点集
	};

}
