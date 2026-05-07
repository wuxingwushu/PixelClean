#pragma once
#include "BaseStruct.hpp"
#include <cmath>


#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

// 运算符优先级提示：移位 > 赋值 > 大小比较 > 加法 > 减法 > 乘法 > 取模 > 除法

namespace PhysicsBlock
{

	/**
	 * @brief   double 转 int，向下取整
	 * @param   val 需要转换的 double 值
	 * @return  返回转换结果
	 * @details 正数直接截断小数部分，负数需要额外减 1（向更负方向取整）
	 */
	int inline ToInt(double val)
	{
		return val >= 0 ? static_cast<int>(val) : (static_cast<int>(val) - 1);
	}

	/**
	 * @brief   float 转 int，向下取整
	 * @param   val 需要转换的 float 值
	 * @return  返回转换结果
	 * @details 正数直接截断小数部分，负数需要额外减 1（向更负方向取整）
	 */
	int inline ToInt(float val)
	{
		return val >= 0 ? static_cast<int>(val) : (static_cast<int>(val) - 1);
	}

	/**
	 * @brief   vec2 转 ivec2，向下取整
	 * @param   val 需要转换的 vec2 值
	 * @return  返回转换结果
	 * @details 分别对 x 和 y 分量调用 ToInt
	 */
	glm::ivec2 inline ToInt(glm::vec2 val)
	{
		return {ToInt(val.x), ToInt(val.y)};
	}

	/**
	 * @brief   dvec2 转 ivec2，向下取整
	 * @param   val 需要转换的 dvec2 值
	 * @return  返回转换结果
	 * @details 分别对 x 和 y 分量调用 ToInt
	 */
	glm::ivec2 inline ToInt(glm::dvec2 val)
	{
		return {ToInt(val.x), ToInt(val.y)};
	}

	/**
	 * @brief 快速平方根（牛顿迭代法近似）
	 * @param S 输入值
	 * @return 平方根近似值
	 * @warning 因为现代CPU有专门的开平方根指令，所以此函数可能比标准 sqrt() 慢
	 */
	float q_sqrt(float S);

	/**
	 * @brief 快速平方根（牛顿迭代法近似）
	 * @param S 输入值
	 * @return 平方根近似值
	 * @warning 因为现代CPU有专门的开平方根指令，所以此函数可能比标准 sqrt() 慢
	 */
	double q_sqrt(double S);

	/**
	 * @brief   获取向量的模的平方
	 * @param   Modulus 输入向量
	 * @return  返回 x*x + y*y
	 * @details 避免开方运算，用于比较距离时的优化
	 */
	FLOAT_ inline ModulusLength(const Vec2_ &Modulus)
	{
		return ((Modulus.x * Modulus.x) + (Modulus.y * Modulus.y));
	}

	/**
	 * @brief   获取向量的模的长度
	 * @param   Modulus 输入向量
	 * @return  返回 sqrt(x*x + y*y)
	 */
	FLOAT_ inline Modulus(const Vec2_ &Modulus)
	{
		return SQRT_((Modulus.x * Modulus.x) + (Modulus.y * Modulus.y));
	}

	/**
	 * @brief   根据角度计算对应的 cos 和 sin 值
	 * @param   angle 角度（弧度）
	 * @return  返回 Vec2_{cos(angle), sin(angle)}
	 */
	Vec2_ inline AngleFloatToAngleVec(FLOAT_ angle)
	{
		return {cos(angle), sin(angle)};
	}

	/**
	 * @brief   根据向量计算与 X轴正向的夹角
	 * @param   XYedge 输入向量
	 * @return  返回向量与 X轴正向的夹角（弧度）
	 * @details 特殊处理 x=0 的情况，使用 acos 计算角度，范围为 [-π, π]
	 */
	FLOAT_ inline EdgeVecToCosAngleFloat(Vec2_ XYedge)
	{
		if (XYedge.x == 0)
		{
			if (XYedge.y == 0)
			{
				return 0; // 分母不可以为零
			}
			return XYedge.y < 0 ? -M_PI_2 : M_PI_2;
		}
		FLOAT_ BeveledEdge = acos(XYedge.x / Modulus(XYedge));
		return XYedge.y < 0 ? -BeveledEdge : BeveledEdge;
	}

	/**
	 * @brief   vec2 基于原点旋转
	 * @param   pos 需要旋转的坐标
	 * @param   angle 旋转角度（弧度）
	 * @return  返回旋转结果
	 * @details 先计算 cos 和 sin，然后调用重载版本
	 */
	Vec2_ vec2angle(Vec2_ pos, FLOAT_ angle);

	/**
	 * @brief   vec2 基于原点旋转（使用预计算的 cos/sin）
	 * @param   pos 需要旋转的坐标
	 * @param   angle Vec2_ 格式，X=cos(angle), Y=sin(angle)
	 * @return  返回旋转结果
	 * @details 实现旋转矩阵效果：x' = x*cos - y*sin, y' = x*sin + y*cos
	 */
	Vec2_ inline vec2angle(Vec2_ pos, Vec2_ angle)
	{
		return Vec2_((pos.x * angle.x) - (pos.y * angle.y), (pos.x * angle.y) + (pos.y * angle.x));
	}

	/**
	 * @brief   vec2 基于某个坐标旋转
	 * @param   pos 需要旋转的点坐标
	 * @param   lingdian 旋转中心的点坐标
	 * @param   angle 旋转角度（弧度）
	 * @return  返回旋转结果
	 * @details 先平移到原点，旋转，再平移回去
	 */
	Vec2_ inline vec2PosAngle(Vec2_ pos, Vec2_ lingdian, FLOAT_ angle)
	{
		return vec2angle(pos - lingdian, angle) + lingdian;
	}

	/**
	 * @brief   vec2 基于某个坐标旋转（使用预计算的 cos/sin）
	 * @param   pos 需要旋转的点坐标
	 * @param   lingdian 旋转中心的点坐标
	 * @param   angle Vec2_ 格式，X=cos(angle), Y=sin(angle)
	 * @return  返回旋转结果
	 * @details 先平移到原点，旋转，再平移回去
	 */
	Vec2_ inline vec2PosAngle(Vec2_ pos, Vec2_ lingdian, Vec2_ angle)
	{
		return vec2angle(pos - lingdian, angle) + lingdian;
	}

	/**
	 * @brief   坐标点转换坐标系后取整
	 * @param   Pos 需要转换的点坐标
	 * @param   xPos 两个坐标系的距离差
	 * @param   angle 两个坐标系的角度差（弧度）
	 * @return  返回转换后坐标系后的整数坐标
	 */
	glm::ivec2 inline ToIntPos(Vec2_ Pos, Vec2_ xPos, FLOAT_ angle)
	{
		return ToInt(vec2angle(xPos - Pos, angle));
	}

	/**
	 * @brief   坐标点转换坐标系后取整（使用预计算的 cos/sin）
	 * @param   sPos 需要转换的点坐标
	 * @param   ePos 两个坐标系的距离差
	 * @param   angle Vec2_ 格式，X=cos(angle), Y=sin(angle)
	 * @return  返回转换后坐标系后的整数坐标
	 */
	glm::ivec2 inline ToIntPos(Vec2_ sPos, Vec2_ ePos, Vec2_ angle)
	{
		return ToInt(vec2angle(ePos - sPos, angle));
	}

	/**
	 * @brief   求线段在 X 为某值处的交点
	 * @param   Apos 线段上的任一点 A
	 * @param   Bpos 线段上的任一点 B
	 * @param   x 设置 X 值
	 * @return  返回线段在 X=x 处的交点
	 * @details 使用线性插值计算交点的 Y 坐标
	 */
	Vec2_ inline LineXToPos(Vec2_ Apos, Vec2_ Bpos, FLOAT_ x)
	{
		FLOAT_ bl = (Bpos.x - x) / (Bpos.x - Apos.x);
		return {x, (Apos.y - Bpos.y) * bl + Bpos.y};
	}

	/**
	 * @brief   求线段在 Y 为某值处的交点
	 * @param   Apos 线段上的任一点 A
	 * @param   Bpos 线段上的任一点 B
	 * @param   y 设置 Y 值
	 * @return  返回线段在 Y=y 处的交点
	 * @details 使用线性插值计算交点的 X 坐标
	 */
	Vec2_ inline LineYToPos(Vec2_ Apos, Vec2_ Bpos, FLOAT_ y)
	{
		FLOAT_ bl = (Bpos.y - y) / (Apos.y - Bpos.y);
		return {(Bpos.x - Apos.x) * bl + Bpos.x, y};
	}

	/**
	 * @brief   角度矩阵旋转器
	 * @details 预计算 cos 和 sin 值，避免重复计算，提高旋转性能
	 */
	struct AngleMat
	{
		FLOAT_ Cos;  // 余弦值
		FLOAT_ Sin;  // 正弦值

		/**
		 * @brief 构造函数，初始化旋转角度
		 * @param angle 角度（弧度）
		 */
		AngleMat(FLOAT_ angle)
		{
			Cos = cos(angle);
			Sin = sin(angle);
		}

		/**
		 * @brief 设置新的旋转角度
		 * @param angle 角度（弧度）
		 */
		void inline SetAngle(FLOAT_ angle)
		{
			Cos = cos(angle);
			Sin = sin(angle);
		}

		/**
		 * @brief 顺时针旋转角度（弧度）
		 * @param pos 位置
		 * @return 旋转结果
		 * @note 绕原点 (0, 0) 旋转
		 */
		Vec2_ inline Rotary(Vec2_ pos)
		{
			return Vec2_((pos.x * Cos) - (pos.y * Sin), (pos.x * Sin) + (pos.y * Cos));
		}

		/**
		 * @brief 逆时针旋转角度（弧度）
		 * @param pos 位置
		 * @return 旋转结果
		 * @note 绕原点 (0, 0) 旋转，相当于旋转 -angle
		 */
		Vec2_ inline Anticlockwise(Vec2_ pos)
		{
			return Vec2_((pos.x * Cos) + (pos.y * Sin), -(pos.x * Sin) + (pos.y * Cos));
		}
	};

	/**
	 * @brief   裁剪出被矩形覆盖的线段
	 * @param   start 起始位置
	 * @param   end 结束位置
	 * @param   width 矩形宽度
	 * @param   height 矩形高度
	 * @return  SquareFocus 结构体，包含是否相交和裁剪后的起止位置
	 * @note    矩形是以原点 (0, 0) 和 (width, height) 为对角的矩形
	 * @note    基于 LineXToPos 和 LineYToPos 推算得来
	 */
	SquareFocus LineSquareFocus(Vec2_ start, Vec2_ end, const FLOAT_ width, const FLOAT_ height);

	/**
	 * @brief   正方形和正方形的碰撞检测（A为静态刚体，B为动态刚体）
	 * @param   posA 正方形 A 的位置（中心坐标）
	 * @param   dA 正方形 A 的边长
	 * @param   angleA 正方形 A 的角度（弧度）
	 * @param   posB 正方形 B 的位置（中心坐标）
	 * @param   dB 正方形 B 的边长
	 * @param   angleB 正方形 B 的角度（弧度）
	 * @return  返回 B 应该偏移的距离（用于碰撞响应）
	 */
	Vec2_ SquareToSquare(Vec2_ posA, FLOAT_ dA, FLOAT_ angleA, Vec2_ posB, FLOAT_ dB, FLOAT_ angleB);

	/**
	 * @brief   矩形和点的碰撞检测
	 * @param   A1 矩形最小 x
	 * @param   A2 矩形最大 x
	 * @param   B1 矩形最小 y
	 * @param   B2 矩形最大 y
	 * @param   Drop 点的位置坐标
	 * @param   PY 期望被移动的方向
	 * @return  返回点应该移动的距离（用于碰撞响应）
	 */
	Vec2_ SquareToDrop(FLOAT_ A1, FLOAT_ A2, FLOAT_ B1, FLOAT_ B2, Vec2_ Drop, Vec2_ PY);

	/**
	 * @brief   正方形和点的碰撞检测（中心在原点）
	 * @param   R 正方形边长的一半
	 * @param   Drop 点的位置坐标
	 * @param   PY 期望被移动的方向
	 * @return  返回点应该移动的距离（用于碰撞响应）
	 * @note    正方形中心在原点，边长为 2*R
	 */
	Vec2_ SquareToDrop(FLOAT_ R, Vec2_ Drop, Vec2_ PY);

	/**
	 * @brief   正方形和射线的碰撞检测
	 * @param   A1 矩形最小 x
	 * @param   A2 矩形最大 x
	 * @param   B1 矩形最小 y
	 * @param   B2 矩形最大 y
	 * @param   Drop 射线起点
	 * @param   PY 射线方向
	 * @return  返回射线应该移动的距离（用于碰撞响应）
	 */
	Vec2_ SquareToRadial(FLOAT_ A1, FLOAT_ A2, FLOAT_ B1, FLOAT_ B2, Vec2_ Drop, Vec2_ PY);

	/**
	 * @brief   扭矩计算
	 * @param   Barycenter 质心位置
	 * @param   Spot 受力点位置
	 * @param   Force 力向量
	 * @return  返回扭矩大小
	 * @note    扭矩 = 力臂 × 力（叉积）
	 */
	FLOAT_ TorqueCalculate(Vec2_ Barycenter, Vec2_ Spot, Vec2_ Force);

	/**
	 * @brief   计算两个向量的夹角
	 * @param   V1 向量1
	 * @param   V2 向量2
	 * @return  返回两个向量的夹角（弧度）
	 */
	FLOAT_ CalculateIncludedAngle(Vec2_ V1, Vec2_ V2);

	/**
	 * @brief   求解分解力（根据向量方向）
	 * @param   angle 力臂方向向量
	 * @param   force 受力向量
	 * @return  返回分解力结构体，包含平行分量和垂直分量
	 */
	DecompositionForce CalculateDecompositionForce(Vec2_ angle, Vec2_ force);

	/**
	 * @brief   求解分解力（根据角度）
	 * @param   angle 力臂角度（弧度）
	 * @param   force 受力向量
	 * @return  返回分解力结构体，包含平行分量和垂直分量
	 */
	DecompositionForce CalculateDecompositionForce(FLOAT_ angle, Vec2_ force);

	/**
	 * @brief   求解分解力的大小（根据向量方向）
	 * @param   angle 力臂方向向量
	 * @param   force 受力向量
	 * @return  返回分解力的大小，包含平行分量和垂直分量的大小
	 */
	DecompositionForceVal CalculateDecompositionForceVal(Vec2_ angle, Vec2_ force);

	/**
	 * @brief   莫顿码（Z-order曲线）
	 * @param   x x 坐标
	 * @param   y y 坐标
	 * @return  返回一维索引值
	 * @note    让二维索引相邻的一维也尽可能相邻，用于空间索引优化
	 */
	uint_fast32_t Morton2D(uint_fast16_t x, uint_fast16_t y);

	/**
	 * @brief   点到线段的最短距离
	 * @param   start 线段起点
	 * @param   end 线段终点
	 * @param   drop 点位置
	 * @return  返回点到线段的垂直最短距离
	 */
	FLOAT_ DropUptoLineShortes(Vec2_ start, Vec2_ end, Vec2_ drop);

	/**
	 * @brief   点到直线的最短距离的交点
	 * @param   start 直线上一点
	 * @param   end 直线上另一点
	 * @param   drop 点位置
	 * @return  返回点到直线的垂直交点
	 * @note    交点可能在线段延长线上
	 */
	Vec2_ DropLineShortesIntersect(Vec2_ start, Vec2_ end, Vec2_ drop);

	/**
	 * @brief   点到线段的最短距离的交点（限制在线段上）
	 * @param   start 线段起点
	 * @param   end 线段终点
	 * @param   drop 点位置
	 * @return  返回点到线段的垂直交点
	 * @note    交点始终在线段上，如果垂足在线段外则返回最近的端点
	 */
	Vec2_ DropUptoLineShortesIntersect(Vec2_ start, Vec2_ end, Vec2_ drop);

	/**
	 * @brief   向量点积
	 * @param   a 向量a
	 * @param   b 向量b
	 * @return  返回 a · b = a.x*b.x + a.y*b.y
	 */
	FLOAT_ inline Dot(const Vec2_ &a, const Vec2_ &b)
	{
		return a.x * b.x + a.y * b.y;
	}

	/**
	 * @brief   向量叉积（二维）
	 * @param   a 向量a
	 * @param   b 向量b
	 * @return  返回 a × b = a.x*b.y - a.y*b.x
	 * @note    结果的绝对值表示平行四边形的面积，符号表示方向
	 */
	FLOAT_ inline Cross(const Vec2_ &a, const Vec2_ &b)
	{
		return a.x * b.y - a.y * b.x;
	}

	/**
	 * @brief   向量顺时针旋转90度并缩放
	 * @param   a 输入向量
	 * @param   s 缩放因子
	 * @return  返回旋转并缩放后的向量
	 */
	Vec2_ inline Cross(const Vec2_ &a, FLOAT_ s)
	{
		return Vec2_(s * a.y, -s * a.x);
	}

	/**
	 * @brief   向量逆时针旋转90度并缩放
	 * @param   s 缩放因子
	 * @param   a 输入向量
	 * @return  返回旋转并缩放后的向量
	 */
	Vec2_ inline Cross(FLOAT_ s, const Vec2_ &a)
	{
		return Vec2_(-s * a.y, s * a.x);
	}

	/**
	 * @brief   将值限制在合理范围内
	 * @param   a 输入值
	 * @param   low 最小值
	 * @param   high 最大值
	 * @return  如果 a 在 [low, high] 范围内返回 a，否则返回最近的边界值
	 */
	FLOAT_ Clamp(FLOAT_ a, FLOAT_ low, FLOAT_ high);

	/**
	 * @brief   生成指定范围内的随机浮点数
	 * @param   lo 最小值
	 * @param   hi 最大值
	 * @return  返回 [lo, hi) 范围内的随机浮点数
	 */
	FLOAT_ inline Random(FLOAT_ lo, FLOAT_ hi)
	{
		FLOAT_ r = (FLOAT_)rand();
		r /= RAND_MAX;
		r = (hi - lo) * r + lo;
		return r;
	}
}
