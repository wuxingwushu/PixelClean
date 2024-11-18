#pragma once
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

		virtual ~ObjectCollision();


		/*++++++++++++++++++++++++++++++++       碰撞有关       ++++++++++++++++++++++++++++++++*/

		//计算当前碰撞体的 外骨架
		virtual void OutlineCalculate();

		//判断点是否符合骨架的条件
		virtual inline void OutlinePointJudge(int x, int y);
		
		//获取点信息
		inline bool PixelAttributeCollision(unsigned int x, unsigned int y) noexcept {
			if ((x >= mNumber.x) || (y >= mNumber.y))
			{
				return false;
			}
			return at({x,y})->Collision;
		}

		//计算重心
		void CalculationBarycenter();

		//有多少个外骨架点
		virtual inline unsigned int GetOutlinePointSize() noexcept { return OutlinePointSize; }
		//获取第 I 个外骨架点
		virtual inline glm::dvec2 GetOutlinePointSet(unsigned int i) noexcept { return { mOutlinePointSet[i].x - Origin.x, mOutlinePointSet[i].y - Origin.y }; }


		//点对玩家的碰撞判断
		CollisionInfo PixelCollision(glm::dvec2 dian);

		//路径碰撞判断
		
		//（输入是 SquarePhysics 坐标的两个点，返回是 SquarePhysics 坐标点， 偏移方向）
		[[nodiscard]] CollisionInfo SquarePhysicsCoordinateSystemRadialCollisionDetection(glm::dvec2 Start, glm::dvec2 End, glm::vec2 Direction);

		//（输入是 SquarePhysics 坐标的两个点，返回是 mPixelAttributeS 数组索引）
		[[nodiscard]] CollisionInfo RelativeCoordinateSystemRadialCollisionDetection(glm::dvec2 Start, glm::dvec2 End);

		

	private:
		unsigned int OutlinePointSize = 0;//点集的数量
		glm::dvec2 mOutlinePointSet[256]{};//外包裹点集
	};

}
