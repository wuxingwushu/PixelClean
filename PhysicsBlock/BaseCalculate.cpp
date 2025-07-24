#define _USE_MATH_DEFINES // 这个要放在第一行
#include <cmath>
#include "BaseCalculate.hpp"
#include <utility>

namespace PhysicsBlock
{

	int ToInt(double val)
	{
		return val >= 0 ? ((int)val) : (((int)val) - 1);
	}

	int ToInt(float val)
	{
		return val >= 0 ? ((int)val) : (((int)val) - 1);
	}

	glm::ivec2 ToInt(glm::vec2 val)
	{
		return {ToInt(val.x), ToInt(val.y)};
	}

	glm::ivec2 ToInt(glm::dvec2 val)
	{
		return {ToInt(val.x), ToInt(val.y)};
	}

	float q_sqrt(float S)
	{
		int i = *(int *)&S;
		i = 0x5f3759df - (i >> 1);
		float y = *(float *)&i;
		return y * (1.5f - 0.5f * S * y * y);
	}

	double q_sqrt(double S)
	{
		int i = *(int *)&S;
		i = 0x5fe6eb50c7b537a9LL - (i >> 1);
		double y = *(double *)&i;
		return y * (1.5f - 0.5f * S * y * y);
	}

	// 获取模的长度(没有开方)
	FLOAT_ ModulusLength(Vec2_ Modulus)
	{
		return ((Modulus.x * Modulus.x) + (Modulus.y * Modulus.y));
	}

	// 获取模的长度
	FLOAT_ Modulus(Vec2_ Modulus)
	{
		return sqrt((Modulus.x * Modulus.x) + (Modulus.y * Modulus.y));
	}

	// 角度，计算对应的cos, sin
	Vec2_ AngleFloatToAngleVec(FLOAT_ angle)
	{
		return {cos(angle), sin(angle)};
	}

	// 根据XY算出cos的角度
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

	// vec2旋转
	Vec2_ vec2angle(Vec2_ pos, FLOAT_ angle)
	{
		FLOAT_ cosangle = cos(angle);
		FLOAT_ sinangle = sin(angle);
		return vec2angle(pos, {cosangle, sinangle});
	}

	// vec2旋转
	Vec2_ vec2angle(Vec2_ pos, Vec2_ angle)
	{
		// 实现旋转矩阵效果
		return Vec2_((pos.x * angle.x) - (pos.y * angle.y), (pos.x * angle.y) + (pos.y * angle.x));
	}

	Vec2_ vec2PosAngle(Vec2_ pos, Vec2_ lingdian, FLOAT_ angle)
	{
		return vec2angle(pos - lingdian, angle) + lingdian;
	}

	Vec2_ vec2PosAngle(Vec2_ pos, Vec2_ lingdian, Vec2_ angle)
	{
		return vec2angle(pos - lingdian, angle) + lingdian;
	}

	glm::ivec2 ToIntPos(Vec2_ Pos, Vec2_ xPos, FLOAT_ angle)
	{
		return ToInt(vec2angle(xPos - Pos, angle));
	}

	glm::ivec2 ToIntPos(Vec2_ sPos, Vec2_ ePos, Vec2_ angle)
	{
		return ToInt(vec2angle(ePos - sPos, angle));
	}

	Vec2_ LineXToPos(Vec2_ Apos, Vec2_ Bpos, FLOAT_ x)
	{
		FLOAT_ bl = (Bpos.x - x) / (Bpos.x - Apos.x);
		return {x, (Apos.y - Bpos.y) * bl + Bpos.y};
	}

	Vec2_ LineYToPos(Vec2_ Apos, Vec2_ Bpos, FLOAT_ y)
	{
		FLOAT_ bl = (Bpos.y - y) / (Apos.y - Bpos.y);
		return {(Bpos.x - Apos.x) * bl + Bpos.x, y};
	}

	SquareFocus LineSquareFocus(Vec2_ start, Vec2_ end, const FLOAT_ width, const FLOAT_ height)
	{
		SquareFocus data{true, start, end};
		if (end.x == start.x)
		{
			if ((start.x > 0) && (start.x <= width))
			{
				if (start.y < 0)
				{
					start.y = 0;
				}
				else if (start.y > height)
				{
					start.y = height;
				}
				if (end.y < 0)
				{
					end.y = 0;
				}
				else if (end.y > height)
				{
					end.y = height;
				}
				if (start.y == end.y)
				{
					data.Focus = false;
				}
				else
				{
					data.start = start;
					data.end = end;
				}
			}
			else
			{
				data.Focus = false;
			}
			return data;
		}
		if (start.y == end.y)
		{
			if ((start.y > 0) && (start.y <= height))
			{
				if (start.x < 0)
				{
					start.x = 0;
				}
				else if (start.x > width)
				{
					start.x = width;
				}
				if (end.x < 0)
				{
					end.x = 0;
				}
				else if (end.x > width)
				{
					end.x = width;
				}
				if (start.x == end.x)
				{
					data.Focus = false;
				}
				else
				{
					data.start = start;
					data.end = end;
				}
			}
			else
			{
				data.Focus = false;
			}
			return data;
		}
		FLOAT_ Difference = (end.x - start.x) / (start.y - end.y);
		FLOAT_ posy1 = -1;
		FLOAT_ posy2 = -1;
		FLOAT_ posx1 = -1;
		FLOAT_ posx2 = -1;

		if (data.start.x < 0)
		{
			posy1 = (end.x / Difference) + end.y;
			data.start = {0, posy1};
		}
		else if (data.start.x > width)
		{
			posy2 = ((end.x - width) / Difference) + end.y;
			data.start = {width, posy2};
		}
		if (data.start.y < 0)
		{
			posx1 = (Difference * end.y) + end.x;
			data.start = {posx1, 0};
		}
		else if (data.start.y > height)
		{
			posx2 = (Difference * (end.y - height)) + end.x;
			data.start = {posx2, height};
		}

		if (data.end.x < 0)
		{
			if (posy1 < 0)
			{
				posy1 = (end.x / Difference) + end.y;
			}
			data.end = {0, posy1};
		}
		else if (data.end.x > width)
		{
			if (posy2 < 0)
			{
				posy2 = ((end.x - width) / Difference) + end.y;
			}
			data.end = {width, posy2};
		}
		if (data.end.y < 0)
		{
			if (posx1 < 0)
			{
				posx1 = (Difference * end.y) + end.x;
			}
			data.end = {posx1, 0};
		}
		else if (data.end.y > height)
		{
			if (posx2 < 0)
			{
				posx2 = (Difference * (end.y - height)) + end.x;
			}
			data.end = {posx2, height};
		}

		if (data.end == data.start)
		{
			data.Focus = false;
		}
		return data;
	}

	// 正方形和正方形的碰撞检测（A为静态刚体，B为动态刚体）
	Vec2_ SquareToSquare(Vec2_ posA, FLOAT_ dA, FLOAT_ angleA, Vec2_ posB, FLOAT_ dB, FLOAT_ angleB)
	{
		FLOAT_ rA = dA / 2; // 边长的一半
		FLOAT_ rB = dB / 2;
		Vec2_ Pos{posB - posA};					 // 两个正方形的  X  Y  的间距
		if (Modulus(Pos) > ((rA + rB) * 1.415f)) // 距離絕對碰不到
		{
			return Vec2_(0, 0);
		}
		FLOAT_ Angle = angleB - angleA; // 两个正方形的角度差
		Angle += M_PI_4;
		FLOAT_ cosangle = cos(Angle);
		FLOAT_ sinangle = sin(Angle);

		FLOAT_ Acosangle = cos(angleA);
		FLOAT_ Asinangle = sin(angleA);
		FLOAT_ SquareR = rB * 1.4142135623730951;
		Vec2_ PYDrop = {SquareR * cosangle, SquareR * sinangle};

		Vec2_ QPYpos{0.0f, 0.0f};							  // 补偿距离
		Vec2_ QPos = vec2angle(Pos, {Acosangle, -Asinangle}); // 以 A 为坐标系，B 正方形的四个点进行判断是否在内
		QPYpos += SquareToDrop(rA, (QPos - PYDrop), QPos);
		QPYpos += SquareToDrop(rA, (QPos + QPYpos + Vec2_{PYDrop.y, -PYDrop.x}), QPos);
		QPYpos += SquareToDrop(rA, (QPos + QPYpos + Vec2_{-PYDrop.y, PYDrop.x}), QPos);
		QPYpos += SquareToDrop(rA, (QPos + QPYpos + PYDrop), QPos);

		QPYpos = vec2angle(QPYpos, {Acosangle, Asinangle});

		sinangle = -sinangle;
		FLOAT_ Bcosangle = cos(angleB);
		FLOAT_ Bsinangle = sin(angleB);
		SquareR = rA * 1.4142135623730951;
		PYDrop = {SquareR * cosangle, SquareR * sinangle};

		Vec2_ HPYpos{0.0f, 0.0f};										  // 补偿距离
		Vec2_ HPos = vec2angle(-(QPYpos + Pos), {Bcosangle, -Bsinangle}); // 以 B 为坐标系，A 正方形的四个点进行判断是否在内
		HPYpos -= SquareToDrop(rB, (HPos - PYDrop), HPos);
		HPYpos -= SquareToDrop(rB, (HPos + HPYpos + Vec2_{PYDrop.y, -PYDrop.x}), HPos);
		HPYpos -= SquareToDrop(rB, (HPos + HPYpos + Vec2_{-PYDrop.y, PYDrop.x}), HPos);
		HPYpos -= SquareToDrop(rB, (HPos + HPYpos + PYDrop), HPos);

		return QPYpos + vec2angle(HPYpos, {Bcosangle, Bsinangle}); // 返回需要矫正的距离
	}

	// 正方形和点的碰撞检测
	Vec2_ SquareToDrop(FLOAT_ A1, FLOAT_ A2, FLOAT_ B1, FLOAT_ B2, Vec2_ Drop, Vec2_ PY)
	{
		Vec2_ PYpos{0.0f, 0.0f};
		if (((Drop.x >= A1) && (Drop.x <= A2)) && ((Drop.y >= B1) && (Drop.y <= B2)))
		{ // 判断这个点是否在另外一个正方形里面
			if (PY.x > 0)
			{
				PYpos.x = A2 - Drop.x;
			}
			else
			{
				PY.x = -PY.x;
				PYpos.x = A1 - Drop.x;
			}

			if (PY.y > 0)
			{
				PYpos.y = B2 - Drop.y;
			}
			else
			{
				PY.y = -PY.y;
				PYpos.y = B1 - Drop.y;
			}

			if (PY.x < PY.y)
			{
				PYpos.x = 0;
			}
			else
			{
				PYpos.y = 0;
			}
		}
		return PYpos;
	}

	Vec2_ SquareToDrop(FLOAT_ R, Vec2_ Drop, Vec2_ PY)
	{
		return SquareToDrop(-R, R, -R, R, Drop, PY);
	}

	Vec2_ SquareToRadial(FLOAT_ A1, FLOAT_ A2, FLOAT_ B1, FLOAT_ B2, Vec2_ Drop, Vec2_ PY)
	{
		Vec2_ PYpos{0.0f, 0.0f};
		FLOAT_ angle = EdgeVecToCosAngleFloat(PY);
		if (((Drop.x >= A1) && (Drop.x <= A2)) && ((Drop.y >= B1) && (Drop.y <= B2)))
		{ // 判断这个点是否在另外一个正方形里面
			if (PY.x > 0)
			{
				PYpos.x = A2 - Drop.x;
			}
			else
			{
				PY.x = -PY.x;
				PYpos.x = A1 - Drop.x;
			}

			if (PY.y > 0)
			{
				PYpos.y = B2 - Drop.y;
			}
			else
			{
				PY.y = -PY.y;
				PYpos.y = B1 - Drop.y;
			}

			if (PY.x < PY.y)
			{
				PYpos.x = 0;
			}
			else
			{
				PYpos.y = 0;
			}
		}
		angle -= EdgeVecToCosAngleFloat(PYpos);
		PYpos /= fabs(cos(angle));
		PYpos = vec2angle(PYpos, angle);
		return PYpos;
	}

	// 扭矩计算
	FLOAT_ TorqueCalculate(Vec2_ Barycenter, Vec2_ Spot, Vec2_ Force)
	{
		glm::vec2 MoveAngle = Barycenter - Spot;
		FLOAT_ AngleA = EdgeVecToCosAngleFloat(MoveAngle);
		FLOAT_ AngleB = EdgeVecToCosAngleFloat(Force);
		FLOAT_ Length = Modulus(MoveAngle);
		return Modulus(Force) * Length * sin(AngleA - AngleB);
	}

	// 求解分解力
	DecompositionForce CalculateDecompositionForce(Vec2_ angle, Vec2_ force)
	{
		FLOAT_ ObjectAngle = EdgeVecToCosAngleFloat(angle); // 获得力臂角度
		return CalculateDecompositionForce(ObjectAngle, force);
	}

	DecompositionForce CalculateDecompositionForce(FLOAT_ angle, Vec2_ force)
	{
		FLOAT_ Fangle = EdgeVecToCosAngleFloat(force);
		FLOAT_ Angle = angle - Fangle;			 // 相差角度
		FLOAT_ Long = Modulus(force);			 // 力大小
		FLOAT_ ParallelLong = Long * cos(Angle); // 平行力大小
		FLOAT_ VerticalLong = Long * sin(Angle); // 垂直力大小
		FLOAT_ CosL = cos(angle);
		FLOAT_ SinL = sin(angle);
		// return { vec2angle({0, -VerticalLong}, {CosL, SinL}), vec2angle({ParallelLong, 0}, {CosL, SinL}) }; // 旋转到世界坐标轴
		return {{VerticalLong * SinL, -VerticalLong * CosL}, {ParallelLong * CosL, ParallelLong * SinL}}; // 旋转到世界坐标轴
	}

	DecompositionForceVal CalculateDecompositionForceVal(Vec2_ angle, Vec2_ force)
	{
		FLOAT_ ObjectAngle = EdgeVecToCosAngleFloat(angle);			// 获得力臂角度
		FLOAT_ Angle = ObjectAngle - EdgeVecToCosAngleFloat(force); // 相差角度
		FLOAT_ Long = Modulus(force);								// 力大小
		return {Long * cos(Angle), Long * sin(Angle)};
	}

	// 计算两个向量的夹角
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

	// 莫顿码
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

	// 点到线段最短距离
	FLOAT_ DropUptoLineShortes(Vec2_ start, Vec2_ end, Vec2_ drop)
	{
		FLOAT_ angle = EdgeVecToCosAngleFloat(end - start);
		Vec2_ Shortes = LineYToPos(start, end, drop.y);
		return (drop.x - Shortes.x) * cos(M_PI / 2 - angle);
	}

	// 点到线段最短距离在线段上的交点
	Vec2_ DropUptoLineShortesIntersect(Vec2_ start, Vec2_ end, Vec2_ drop)
	{
		FLOAT_ angle = EdgeVecToCosAngleFloat(end - start);
		Vec2_ Shortes = LineYToPos(start, end, drop.y);
		Vec2_ Intersect = {(drop.x - Shortes.x) * sin(M_PI / 2 - angle), 0};
		Intersect = vec2angle(Intersect, angle);
		return Shortes + Intersect;
	}

	FLOAT_ Dot(const Vec2_ &a, const Vec2_ &b)
	{
		return a.x * b.x + a.y * b.y;
	}
	/**
	 * @brief 投影
	 * @param a
	 * @param b
	 * @return  */
	FLOAT_ Cross(const Vec2_ &a, const Vec2_ &b)
	{
		return a.x * b.y - a.y * b.x;
	}

	// 顺时针 转 90度， s 缩放比
	Vec2_ Cross(const Vec2_ &a, FLOAT_ s)
	{
		return Vec2_(s * a.y, -s * a.x);
	}

	// 逆时针 转 90度， s 缩放比
	Vec2_ Cross(FLOAT_ s, const Vec2_ &a)
	{
		return Vec2_(-s * a.y, s * a.x);
	}

	/**
	 * @brief 返回合理范围内的数
	 * @param a 值
	 * @param low 最低
	 * @param high 最高
	 * @return 合理值
	 * @note a 不合理就返回和他最近的合理值 */
	FLOAT_ Clamp(FLOAT_ a, FLOAT_ low, FLOAT_ high)
	{
		return std::max(low, std::min(a, high));
	}

	FLOAT_ Random(FLOAT_ lo, FLOAT_ hi)
	{
		FLOAT_ r = (FLOAT_)rand();
		r /= RAND_MAX;
		r = (hi - lo) * r + lo;
		return r;
	}
}