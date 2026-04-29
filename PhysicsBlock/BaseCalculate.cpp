#define _USE_MATH_DEFINES // 这个要放在第一行
#include <cmath>
#include "BaseCalculate.hpp"
#include <utility>

namespace PhysicsBlock
{

	/**
	 * @brief   double 转 int，向下取整
	 * @param   val 需要转换的 double 值
	 * @return  返回转换结果
	 * @details 正数直接截断小数部分，负数需要额外减 1（向更负方向取整）
	 */
	int ToInt(double val)
	{
		return val >= 0 ? static_cast<int>(val) : (static_cast<int>(val) - 1);
	}

	/**
	 * @brief   float 转 int，向下取整
	 * @param   val 需要转换的 float 值
	 * @return  返回转换结果
	 * @details 正数直接截断小数部分，负数需要额外减 1（向更负方向取整）
	 */
	int ToInt(float val)
	{
		return val >= 0 ? static_cast<int>(val) : (static_cast<int>(val) - 1);
	}

	/**
	 * @brief   vec2 转 ivec2，向下取整
	 * @param   val 需要转换的 vec2 值
	 * @return  返回转换结果
	 * @details 分别对 x 和 y 分量调用 ToInt
	 */
	glm::ivec2 ToInt(glm::vec2 val)
	{
		return {ToInt(val.x), ToInt(val.y)};
	}

	/**
	 * @brief   dvec2 转 ivec2，向下取整
	 * @param   val 需要转换的 dvec2 值
	 * @return  返回转换结果
	 * @details 分别对 x 和 y 分量调用 ToInt
	 */
	glm::ivec2 ToInt(glm::dvec2 val)
	{
		return {ToInt(val.x), ToInt(val.y)};
	}

	/**
	 * @brief   快速平方根（牛顿迭代法近似）
	 * @param   S 输入值
	 * @return  平方根近似值
	 * @details 使用经典的 Quake III Arena 快速平方根算法
	 * @warning 现代 CPU 有专门的开平方根指令，此函数可能比标准 sqrt() 慢
	 */
	float q_sqrt(float S)
	{
		int i = *reinterpret_cast<int*>(&S);
		i = 0x5f3759df - (i >> 1);
		float y = *reinterpret_cast<float*>(&i);
		return y * (1.5f - 0.5f * S * y * y);
	}

	/**
	 * @brief   快速平方根（牛顿迭代法近似）
	 * @param   S 输入值
	 * @return  平方根近似值
	 * @details 使用经典的 Quake III Arena 快速平方根算法的 double 版本
	 * @warning 现代 CPU 有专门的开平方根指令，此函数可能比标准 sqrt() 慢
	 */
	double q_sqrt(double S)
	{
		long long i = *reinterpret_cast<long long*>(&S);
		i = 0x5fe6eb50c7b537a9LL - (i >> 1);
		double y = *reinterpret_cast<double*>(&i);
		return y * (1.5f - 0.5f * S * y * y);
	}

	/**
	 * @brief   获取向量的模的平方
	 * @param   Modulus 输入向量
	 * @return  返回 x*x + y*y
	 * @details 避免开方运算，用于比较距离时的优化
	 */
	FLOAT_ ModulusLength(const Vec2_ &Modulus)
	{
		return ((Modulus.x * Modulus.x) + (Modulus.y * Modulus.y));
	}

	/**
	 * @brief   获取向量的模的长度
	 * @param   Modulus 输入向量
	 * @return  返回 sqrt(x*x + y*y)
	 */
	FLOAT_ Modulus(const Vec2_ &Modulus)
	{
		return SQRT_((Modulus.x * Modulus.x) + (Modulus.y * Modulus.y));
	}

	/**
	 * @brief   根据角度计算对应的 cos 和 sin 值
	 * @param   angle 角度（弧度）
	 * @return  返回 Vec2_{cos(angle), sin(angle)}
	 */
	Vec2_ AngleFloatToAngleVec(FLOAT_ angle)
	{
		return {cos(angle), sin(angle)};
	}

	/**
	 * @brief   根据向量计算与 X轴正向的夹角
	 * @param   XYedge 输入向量
	 * @return  返回向量与 X轴正向的夹角（弧度）
	 * @details 特殊处理 x=0 的情况，使用 acos 计算角度，范围为 [-π, π]
	 */
	FLOAT_ EdgeVecToCosAngleFloat(Vec2_ XYedge)
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
	Vec2_ vec2angle(Vec2_ pos, FLOAT_ angle)
	{
		FLOAT_ cosangle = cos(angle);
		FLOAT_ sinangle = sin(angle);
		return vec2angle(pos, {cosangle, sinangle});
	}

	/**
	 * @brief   vec2 基于原点旋转（使用预计算的 cos/sin）
	 * @param   pos 需要旋转的坐标
	 * @param   angle Vec2_ 格式，X=cos(angle), Y=sin(angle)
	 * @return  返回旋转结果
	 * @details 实现旋转矩阵效果：x' = x*cos - y*sin, y' = x*sin + y*cos
	 */
	Vec2_ vec2angle(Vec2_ pos, Vec2_ angle)
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
	Vec2_ vec2PosAngle(Vec2_ pos, Vec2_ lingdian, FLOAT_ angle)
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
	Vec2_ vec2PosAngle(Vec2_ pos, Vec2_ lingdian, Vec2_ angle)
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
	glm::ivec2 ToIntPos(Vec2_ Pos, Vec2_ xPos, FLOAT_ angle)
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
	glm::ivec2 ToIntPos(Vec2_ sPos, Vec2_ ePos, Vec2_ angle)
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
	Vec2_ LineXToPos(Vec2_ Apos, Vec2_ Bpos, FLOAT_ x)
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
	Vec2_ LineYToPos(Vec2_ Apos, Vec2_ Bpos, FLOAT_ y)
	{
		FLOAT_ bl = (Bpos.y - y) / (Apos.y - Bpos.y);
		return {(Bpos.x - Apos.x) * bl + Bpos.x, y};
	}

	/**
	 * @brief   裁剪出被矩形覆盖的线段
	 * @param   start 起始位置
	 * @param   end 结束位置
	 * @param   width 矩形宽度
	 * @param   height 矩形高度
	 * @return  SquareFocus 结构体，包含是否相交和裁剪后的起止位置
	 * @details 处理水平、垂直线段的特殊情况，然后裁剪一般线段
	 */
	SquareFocus LineSquareFocus(Vec2_ start, Vec2_ end, const FLOAT_ width, const FLOAT_ height)
	{
		SquareFocus data{true, start, end};
		
		// 处理垂直线段
		if (end.x == start.x)
		{
			if ((start.x > 0) && (start.x <= width))
			{
				// 裁剪 Y 坐标
				if (start.y < 0) start.y = 0;
				else if (start.y > height) start.y = height;
				if (end.y < 0) end.y = 0;
				else if (end.y > height) end.y = height;
				
				if (start.y == end.y) data.Focus = false;
				else { data.start = start; data.end = end; }
			}
			else data.Focus = false;
			return data;
		}
		
		// 处理水平线段
		if (start.y == end.y)
		{
			if ((start.y > 0) && (start.y <= height))
			{
				// 裁剪 X 坐标
				if (start.x < 0) start.x = 0;
				else if (start.x > width) start.x = width;
				if (end.x < 0) end.x = 0;
				else if (end.x > width) end.x = width;
				
				if (start.x == end.x) data.Focus = false;
				else { data.start = start; data.end = end; }
			}
			else data.Focus = false;
			return data;
		}
		
		// 处理一般线段
		FLOAT_ Difference = (end.x - start.x) / (start.y - end.y);
		
		// 裁剪起点
		if (data.start.x < 0) data.start = {0, (end.x / Difference) + end.y};
		else if (data.start.x > width) data.start = {width, ((end.x - width) / Difference) + end.y};
		if (data.start.y < 0) data.start = {(Difference * end.y) + end.x, 0};
		else if (data.start.y > height) data.start = {(Difference * (end.y - height)) + end.x, height};
		
		// 裁剪终点
		if (data.end.x < 0) data.end = {0, (end.x / Difference) + end.y};
		else if (data.end.x > width) data.end = {width, ((end.x - width) / Difference) + end.y};
		if (data.end.y < 0) data.end = {(Difference * end.y) + end.x, 0};
		else if (data.end.y > height) data.end = {(Difference * (end.y - height)) + end.x, height};
		
		if (data.end == data.start) data.Focus = false;
		return data;
	}
	/*
	Liang-Barsky 算法实现
	SquareFocus LineSquareFocus(Vec2_ start, Vec2_ end, const FLOAT_ width, const FLOAT_ height)
	{
		SquareFocus data{false, start, end};

		FLOAT_ x0 = start.x, y0 = start.y;
		FLOAT_ x1 = end.x, y1 = end.y;
		FLOAT_ left = 0, right = width, bottom = 0, top = height;

		FLOAT_ dx = x1 - x0;
		FLOAT_ dy = y1 - y0;

		FLOAT_ p[4] = {-dx, dx, -dy, dy};
		FLOAT_ q[4] = {x0 - left, right - x0, y0 - bottom, top - y0};

		FLOAT_ u1 = 0.0, u2 = 1.0;

		for (int i = 0; i < 4; i++)
		{
			if (p[i] == 0)
			{
				if (q[i] < 0) return data;
			}
			else
			{
				FLOAT_ t = q[i] / p[i];
				if (p[i] < 0)
				{
					if (t > u1) u1 = t;
				}
				else
				{
					if (t < u2) u2 = t;
				}
			}
		}

		if (u1 > u2) return data;

		data.Focus = true;
		data.start = {x0 + u1 * dx, y0 + u1 * dy};
		data.end = {x0 + u2 * dx, y0 + u2 * dy};
		return data;
	}
	*/

	/**
	 * @brief   正方形和正方形的碰撞检测
	 * @param   posA 正方形 A 的位置（中心坐标）
	 * @param   dA 正方形 A 的边长
	 * @param   angleA 正方形 A 的角度（弧度）
	 * @param   posB 正方形 B 的位置（中心坐标）
	 * @param   dB 正方形 B 的边长
	 * @param   angleB 正方形 B 的角度（弧度）
	 * @return  返回 B 应该偏移的距离
	 * @details 使用分离轴定理，检测两个正方形的四个角是否相互侵入
	 */
	Vec2_ SquareToSquare(Vec2_ posA, FLOAT_ dA, FLOAT_ angleA, Vec2_ posB, FLOAT_ dB, FLOAT_ angleB)
	{
		FLOAT_ rA = dA / 2; // 边长的一半
		FLOAT_ rB = dB / 2;
		Vec2_ Pos{posB - posA};
		
		// 快速距离检测
		if (Modulus(Pos) > ((rA + rB) * 1.415f)) return Vec2_(0, 0);
		
		FLOAT_ Angle = angleB - angleA + M_PI_4;
		FLOAT_ cosangle = cos(Angle);
		FLOAT_ sinangle = sin(Angle);
		FLOAT_ Acosangle = cos(angleA);
		FLOAT_ Asinangle = sin(angleA);
		FLOAT_ SquareR = rB * 1.4142135623730951;
		Vec2_ PYDrop = {SquareR * cosangle, SquareR * sinangle};
		
		// 以 A 为坐标系，检测 B 的四个角
		Vec2_ QPYpos{0.0f, 0.0f};
		Vec2_ QPos = vec2angle(Pos, {Acosangle, -Asinangle});
		QPYpos += SquareToDrop(rA, (QPos - PYDrop), QPos);
		QPYpos += SquareToDrop(rA, (QPos + QPYpos + Vec2_{PYDrop.y, -PYDrop.x}), QPos);
		QPYpos += SquareToDrop(rA, (QPos + QPYpos + Vec2_{-PYDrop.y, PYDrop.x}), QPos);
		QPYpos += SquareToDrop(rA, (QPos + QPYpos + PYDrop), QPos);
		QPYpos = vec2angle(QPYpos, {Acosangle, Asinangle});
		
		// 以 B 为坐标系，检测 A 的四个角
		sinangle = -sinangle;
		FLOAT_ Bcosangle = cos(angleB);
		FLOAT_ Bsinangle = sin(angleB);
		SquareR = rA * 1.4142135623730951;
		PYDrop = {SquareR * cosangle, SquareR * sinangle};
		
		Vec2_ HPYpos{0.0f, 0.0f};
		Vec2_ HPos = vec2angle(-(QPYpos + Pos), {Bcosangle, -Bsinangle});
		HPYpos -= SquareToDrop(rB, (HPos - PYDrop), HPos);
		HPYpos -= SquareToDrop(rB, (HPos + HPYpos + Vec2_{PYDrop.y, -PYDrop.x}), HPos);
		HPYpos -= SquareToDrop(rB, (HPos + HPYpos + Vec2_{-PYDrop.y, PYDrop.x}), HPos);
		HPYpos -= SquareToDrop(rB, (HPos + HPYpos + PYDrop), HPos);
		
		return QPYpos + vec2angle(HPYpos, {Bcosangle, Bsinangle});
	}

	/**
	 * @brief   矩形和点的碰撞检测
	 * @param   A1 矩形最小 x
	 * @param   A2 矩形最大 x
	 * @param   B1 矩形最小 y
	 * @param   B2 矩形最大 y
	 * @param   Drop 点的位置坐标
	 * @param   PY 期望被移动的方向
	 * @return  返回点应该移动的距离
	 * @details 判断点是否在矩形内，如果在则计算最小推出距离
	 */
	Vec2_ SquareToDrop(FLOAT_ A1, FLOAT_ A2, FLOAT_ B1, FLOAT_ B2, Vec2_ Drop, Vec2_ PY)
	{
		Vec2_ PYpos{0.0f, 0.0f};
		if (((Drop.x >= A1) && (Drop.x <= A2)) && ((Drop.y >= B1) && (Drop.y <= B2)))
		{
			if (PY.x > 0) PYpos.x = A2 - Drop.x;
			else { PY.x = -PY.x; PYpos.x = A1 - Drop.x; }
			
			if (PY.y > 0) PYpos.y = B2 - Drop.y;
			else { PY.y = -PY.y; PYpos.y = B1 - Drop.y; }
			
			if (PY.x < PY.y) PYpos.x = 0;
			else PYpos.y = 0;
		}
		return PYpos;
	}

	/**
	 * @brief   正方形和点的碰撞检测（中心在原点）
	 * @param   R 正方形边长的一半
	 * @param   Drop 点的位置坐标
	 * @param   PY 期望被移动的方向
	 * @return  返回点应该移动的距离
	 * @details 调用通用版本，矩形范围为 [-R, R]
	 */
	Vec2_ SquareToDrop(FLOAT_ R, Vec2_ Drop, Vec2_ PY)
	{
		return SquareToDrop(-R, R, -R, R, Drop, PY);
	}

	/**
	 * @brief   正方形和射线的碰撞检测
	 * @param   A1 矩形最小 x
	 * @param   A2 矩形最大 x
	 * @param   B1 矩形最小 y
	 * @param   B2 矩形最大 y
	 * @param   Drop 射线起点
	 * @param   PY 射线方向
	 * @return  返回射线应该移动的距离
	 * @details 在 SquareToDrop 基础上，根据射线方向进行修正
	 */
	Vec2_ SquareToRadial(FLOAT_ A1, FLOAT_ A2, FLOAT_ B1, FLOAT_ B2, Vec2_ Drop, Vec2_ PY)
	{
		Vec2_ PYpos{0.0f, 0.0f};
		FLOAT_ angle = EdgeVecToCosAngleFloat(PY);
		
		if (((Drop.x >= A1) && (Drop.x <= A2)) && ((Drop.y >= B1) && (Drop.y <= B2)))
		{
			if (PY.x > 0) PYpos.x = A2 - Drop.x;
			else { PY.x = -PY.x; PYpos.x = A1 - Drop.x; }
			
			if (PY.y > 0) PYpos.y = B2 - Drop.y;
			else { PY.y = -PY.y; PYpos.y = B1 - Drop.y; }
			
			if (PY.x < PY.y) PYpos.x = 0;
			else PYpos.y = 0;
		}
		
		angle -= EdgeVecToCosAngleFloat(PYpos);
		PYpos /= fabs(cos(angle));
		PYpos = vec2angle(PYpos, angle);
		return PYpos;
	}

	/**
	 * @brief   扭矩计算
	 * @param   Barycenter 质心位置
	 * @param   Spot 受力点位置
	 * @param   Force 力向量
	 * @return  返回扭矩大小
	 * @details 扭矩 = |力臂| × |力| × sin(夹角)
	 */
	FLOAT_ TorqueCalculate(Vec2_ Barycenter, Vec2_ Spot, Vec2_ Force)
	{
		glm::vec2 MoveAngle = Barycenter - Spot;
		FLOAT_ AngleA = EdgeVecToCosAngleFloat(MoveAngle);
		FLOAT_ AngleB = EdgeVecToCosAngleFloat(Force);
		FLOAT_ Length = Modulus(MoveAngle);
		return Modulus(Force) * Length * sin(AngleA - AngleB);
	}

	/**
	 * @brief   求解分解力（根据向量方向）
	 * @param   angle 力臂方向向量
	 * @param   force 受力向量
	 * @return  返回分解力结构体
	 * @details 先获取力臂角度，然后调用角度版本
	 */
	DecompositionForce CalculateDecompositionForce(Vec2_ angle, Vec2_ force)
	{
		FLOAT_ ObjectAngle = EdgeVecToCosAngleFloat(angle);
		return CalculateDecompositionForce(ObjectAngle, force);
	}

	/**
	 * @brief   求解分解力（根据角度）
	 * @param   angle 力臂角度（弧度）
	 * @param   force 受力向量
	 * @return  返回分解力结构体，包含平行分量和垂直分量
	 * @details 将力分解为平行于力臂和垂直于力臂的两个分量
	 */
	DecompositionForce CalculateDecompositionForce(FLOAT_ angle, Vec2_ force)
	{
		FLOAT_ Fangle = EdgeVecToCosAngleFloat(force);
		FLOAT_ Angle = angle - Fangle;
		FLOAT_ Long = Modulus(force);
		FLOAT_ ParallelLong = Long * cos(Angle);  // 平行分量
		FLOAT_ VerticalLong = Long * sin(Angle); // 垂直分量
		FLOAT_ CosL = cos(angle);
		FLOAT_ SinL = sin(angle);
		return {{VerticalLong * SinL, -VerticalLong * CosL}, {ParallelLong * CosL, ParallelLong * SinL}};
	}

	/**
	 * @brief   求解分解力的大小
	 * @param   angle 力臂方向向量
	 * @param   force 受力向量
	 * @return  返回分解力的大小
	 */
	DecompositionForceVal CalculateDecompositionForceVal(Vec2_ angle, Vec2_ force)
	{
		FLOAT_ ObjectAngle = EdgeVecToCosAngleFloat(angle);
		FLOAT_ Angle = ObjectAngle - EdgeVecToCosAngleFloat(force);
		FLOAT_ Long = Modulus(force);
		return {static_cast<FLOAT_>(Long * cos(Angle)), static_cast<FLOAT_>(Long * sin(Angle))};
	}

	/**
	 * @brief   计算两个向量的夹角
	 * @param   V1 向量1
	 * @param   V2 向量2
	 * @return  返回两个向量的夹角（弧度）
	 * @details 使用点积公式计算：theta = acos((v1·v2) / (|v1||v2|))
	 */
	FLOAT_ CalculateIncludedAngle(Vec2_ V1, Vec2_ V2)
	{
		// 标准化向量
		Vec2_ normV1 = glm::normalize(V1);
		Vec2_ normV2 = glm::normalize(V2);

		// 计算向量点乘
		FLOAT_ dotProduct = glm::dot(normV1, normV2);

		// 计算夹角（弧度）
		FLOAT_ angleRad = glm::acos(dotProduct);

		// 将弧度转换为角度（可选）
		// FLOAT_ angleDeg = glm::degrees(angleRad);

		return angleRad;
	}

	/**
	 * @brief   莫顿码（Z-order曲线）
	 * @param   x x 坐标
	 * @param   y y 坐标
	 * @return  返回一维索引值
	 * @details 使用位交错算法，让二维索引相邻的一维也尽可能相邻
	 */
	uint_fast32_t Morton2D(uint_fast16_t x, uint_fast16_t y)
	{
		uint_fast64_t m = x | (uint_fast64_t(y) << 32); // put Y in upper 32 bits, X in lower 32 bits
		m = (m | (m << 8)) & 0x00FF00FF00FF00FF;
		m = (m | (m << 4)) & 0x0F0F0F0F0F0F0F0F;
		m = (m | (m << 2)) & 0x3333333333333333;
		m = (m | (m << 1)) & 0x5555555555555555;
		m = m | (m >> 31); // merge X and Y back together
		// hard cut off to 32 bits, because on some systems uint_fast32_t will be a 64-bit type, and we don't want to retain split Y-version in the upper 32 bits.
		m = m & 0x00000000FFFFFFFF;
		return uint_fast32_t(m);
	}

	/**
	 * @brief   点到线段的最短距离
	 * @param   start 线段起点
	 * @param   end 线段终点
	 * @param   drop 点位置
	 * @return  返回点到线段的垂直最短距离
	 */
	FLOAT_ DropUptoLineShortes(Vec2_ start, Vec2_ end, Vec2_ drop)
	{
		Vec2_ Shortes = DropLineShortesIntersect(start, end, drop);
		return Modulus(Shortes);
	}

	/**
	 * @brief   点到直线的最短距离的交点
	 * @param   start 直线上一点
	 * @param   end 直线上另一点
	 * @param   drop 点位置
	 * @return  返回点到直线的垂直交点
	 * @details 使用向量投影计算垂足
	 */
	Vec2_ DropLineShortesIntersect(Vec2_ start, Vec2_ end, Vec2_ drop)
	{
		// 计算向量 AB 和 AP
		Vec2_ AB = end - start;
		Vec2_ AP = drop - start;

		// 计算投影比例 t = (AP · AB) / |AB|²
		FLOAT_ dotProduct = Dot(AB, AP);
		FLOAT_ lenSq = ModulusLength(AB);
		FLOAT_ t = dotProduct / lenSq;

		return start + AB * t;
	}

	/**
	 * @brief   点到线段的最短距离的交点（限制在线段上）
	 * @param   start 线段起点
	 * @param   end 线段终点
	 * @param   drop 点位置
	 * @return  返回点到线段的垂直交点
	 * @details 如果垂足在线段外，返回最近的端点
	 */
	Vec2_ DropUptoLineShortesIntersect(Vec2_ start, Vec2_ end, Vec2_ drop)
	{
		// 计算向量 AB 和 AP
		Vec2_ AB = end - start;
		Vec2_ AP = drop - start;

		// 计算投影比例 t = (AP · AB) / |AB|²
		FLOAT_ dotProduct = Dot(AB, AP);
		FLOAT_ lenSq = ModulusLength(AB);
		FLOAT_ t = dotProduct / lenSq;
		
		if (t <= 0.0) return start;
		else if (t >= 1.0) return end;
		
		return start + AB * t;
	}

	/**
	 * @brief   向量点积
	 * @param   a 向量a
	 * @param   b 向量b
	 * @return  返回 a · b = a.x*b.x + a.y*b.y
	 */
	FLOAT_ Dot(const Vec2_ &a, const Vec2_ &b)
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
	FLOAT_ Cross(const Vec2_ &a, const Vec2_ &b)
	{
		return a.x * b.y - a.y * b.x;
	}

	/**
	 * @brief   向量顺时针旋转90度并缩放
	 * @param   a 输入向量
	 * @param   s 缩放因子
	 * @return  返回旋转并缩放后的向量
	 */
	Vec2_ Cross(const Vec2_ &a, FLOAT_ s)
	{
		return Vec2_(s * a.y, -s * a.x);
	}

	/**
	 * @brief   向量逆时针旋转90度并缩放
	 * @param   s 缩放因子
	 * @param   a 输入向量
	 * @return  返回旋转并缩放后的向量
	 */
	Vec2_ Cross(FLOAT_ s, const Vec2_ &a)
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
	FLOAT_ Clamp(FLOAT_ a, FLOAT_ low, FLOAT_ high)
	{
		return std::max(low, std::min(a, high));
	}

	/**
	 * @brief   生成指定范围内的随机浮点数
	 * @param   lo 最小值
	 * @param   hi 最大值
	 * @return  返回 [lo, hi) 范围内的随机浮点数
	 */
	FLOAT_ Random(FLOAT_ lo, FLOAT_ hi)
	{
		FLOAT_ r = (FLOAT_)rand();
		r /= RAND_MAX;
		r = (hi - lo) * r + lo;
		return r;
	}
}
