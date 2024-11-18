#define _USE_MATH_DEFINES // 这个要放在第一行
#include <cmath>
#include "PhysicsCalculate.h"


namespace SquarePhysics {

	inline int ToInt(double val)
	{
		return val >= 0 ? ((int)val) : (((int)val) - 1);
	}

	inline int ToInt(float val)
	{
		return val >= 0 ? ((int)val) : (((int)val) - 1);
	}

	inline glm::ivec2 ToIvec2(glm::vec2 val)
	{
		return { ToInt(val.x), ToInt(val.y) };
	}

	inline glm::ivec2 ToIvec2(glm::dvec2 val)
	{
		return { ToInt(val.x), ToInt(val.y) };
	}

	//获取模的长度(没有开方)
	double ModulusLength(glm::dvec2 Modulus) {
		return ((Modulus.x * Modulus.x) + (Modulus.y * Modulus.y));
	}

	//获取模的长度
	inline double Modulus(glm::dvec2 Modulus) {
		return sqrt((Modulus.x * Modulus.x) + (Modulus.y * Modulus.y));
	}

	//角度，计算对应的cos, sin 
	glm::dvec2 AngleFloatToAngleVec(double angle) {
		return { cos(angle), sin(angle) };
	}

	//根据XY算出cos的角度
	inline double EdgeVecToCosAngleFloat(glm::dvec2 XYedge) {
		if (XYedge.x == 0)
		{
			if (XYedge.y == 0)
			{
				return 0; // 分母不可以为零
			}
			return XYedge.y < 0 ? -M_PI_2 : M_PI_2;
		}
		double BeveledEdge = acos(XYedge.x / Modulus(XYedge));
		return XYedge.y < 0 ? -BeveledEdge : BeveledEdge;
	}

	//vec2旋转
	inline glm::dvec2 vec2angle(glm::dvec2 pos, double angle) {
		double cosangle = cos(angle);
		double sinangle = sin(angle);
		return glm::dvec2((pos.x * cosangle) - (pos.y * sinangle), (pos.x * sinangle) + (pos.y * cosangle));
	}

	//vec2旋转
	inline glm::dvec2 vec2angle(glm::dvec2 pos, glm::dvec2 angle) {
		return glm::dvec2((pos.x * angle.x) - (pos.y * angle.y), (pos.x * angle.y) + (pos.y * angle.x));
	}

	inline glm::dvec2 vec2PosAngle(glm::dvec2 pos, glm::dvec2 lingdian, double angle)
	{
		return vec2angle(pos - lingdian, angle) + lingdian;
	}

	inline glm::dvec2 vec2PosAngle(glm::dvec2 pos, glm::dvec2 lingdian, glm::dvec2 angle)
	{
		return vec2angle(pos - lingdian, angle) + lingdian;
	}

	inline glm::ivec2 ToIntPos(glm::dvec2 Pos, glm::dvec2 xPos, double angle)
	{
		return ToIvec2(vec2angle(xPos - Pos, angle));
	}

	inline glm::ivec2 ToIntPos(glm::dvec2 sPos, glm::dvec2 ePos, glm::dvec2 angle)
	{
		return ToIvec2(vec2angle(ePos - sPos, angle));
	}

	inline glm::dvec2 LineXToPos(glm::dvec2 Apos, glm::dvec2 Bpos, double x)
	{
		double bl = (Bpos.x - x) / (Bpos.x - Apos.x);
		return { x, (Apos.y - Bpos.y) * bl + Bpos.y };
	}

	inline glm::dvec2 LineYToPos(glm::dvec2 Apos, glm::dvec2 Bpos, double y)
	{
		double bl = (Bpos.y - y) / (Bpos.y - Apos.y);
		return { (Apos.x - Bpos.x) * bl + Bpos.x, y };
	}

	//正方形和正方形的碰撞检测（A为静态刚体，B为动态刚体）
	glm::dvec2 SquareToSquare(glm::dvec2 posA, unsigned int dA, double angleA, glm::dvec2 posB, unsigned int dB, double angleB) {
		double rA = dA / 2; // 边长的一半
		double rB = dB / 2;
		glm::dvec2 Pos{ posB - posA }; // 两个正方形的  X  Y  的间距
		if (Modulus(Pos) > ((rA + rB) * 1.415f))                     // 距離絕對碰不到
		{
			return glm::dvec2(0, 0);
		}
		double Angle = angleB - angleA; // 两个正方形的角度差
		Angle += M_PI_4;
		double cosangle = cos(Angle);
		double sinangle = sin(Angle);

		double Acosangle = cos(angleA);
		double Asinangle = sin(angleA);
		double SquareR = rB * 1.4142135623730951;
		glm::dvec2 PYDrop = { SquareR * cosangle, SquareR * sinangle };

		glm::dvec2 QPYpos{ 0.0f, 0.0f };                             // 补偿距离
		glm::dvec2 QPos = vec2angle(Pos, { Acosangle, -Asinangle }); // 以 A 为坐标系，B 正方形的四个点进行判断是否在内
		QPYpos += SquareToDrop(rA, (QPos - PYDrop), QPos);
		QPYpos += SquareToDrop(rA, (QPos + QPYpos + glm::dvec2{ PYDrop.y, -PYDrop.x }), QPos);
		QPYpos += SquareToDrop(rA, (QPos + QPYpos + glm::dvec2{ -PYDrop.y, PYDrop.x }), QPos);
		QPYpos += SquareToDrop(rA, (QPos + QPYpos + PYDrop), QPos);

		QPYpos = vec2angle(QPYpos, { Acosangle, Asinangle });

		sinangle = -sinangle;
		double Bcosangle = cos(angleB);
		double Bsinangle = sin(angleB);
		SquareR = rA * 1.4142135623730951;
		PYDrop = { SquareR * cosangle, SquareR * sinangle };

		glm::dvec2 HPYpos{ 0.0f, 0.0f };                                         // 补偿距离
		glm::dvec2 HPos = vec2angle(-(QPYpos + Pos), { Bcosangle, -Bsinangle }); // 以 B 为坐标系，A 正方形的四个点进行判断是否在内
		HPYpos -= SquareToDrop(rB, (HPos - PYDrop), HPos);
		HPYpos -= SquareToDrop(rB, (HPos + HPYpos + glm::dvec2{ PYDrop.y, -PYDrop.x }), HPos);
		HPYpos -= SquareToDrop(rB, (HPos + HPYpos + glm::dvec2{ -PYDrop.y, PYDrop.x }), HPos);
		HPYpos -= SquareToDrop(rB, (HPos + HPYpos + PYDrop), HPos);

		return QPYpos + vec2angle(HPYpos, { Bcosangle, Bsinangle }); // 返回需要矫正的距离
	}

	//正方形和点的碰撞检测
	inline glm::dvec2 SquareToDrop(double A1, double A2, double B1, double B2, glm::dvec2 Drop, glm::dvec2 PY) {
		glm::dvec2 PYpos{ 0.0f,0.0f };
		if (((Drop.x >= A1) && (Drop.x <= A2)) && ((Drop.y >= B1) && (Drop.y <= B2))) {//判断这个点是否在另外一个正方形里面
			if (PY.x > 0) {
				PYpos.x = A2 - Drop.x;
			}
			else {
				PY.x = -PY.x;
				PYpos.x = A1 - Drop.x;
			}

			if (PY.y > 0) {
				PYpos.y = B2 - Drop.y;
			}
			else {
				PY.y = -PY.y;
				PYpos.y = B1 - Drop.y;
			}

			if (PY.x < PY.y) {
				PYpos.x = 0;
			}
			else {
				PYpos.y = 0;
			}
		}
		return PYpos;
	}

	inline glm::dvec2 SquareToDrop(double R, glm::dvec2 Drop, glm::dvec2 PY)
	{
		return SquareToDrop(-R, R, -R, R, Drop, PY);
	}

	glm::dvec2 SquareToRadial(double A1, double A2, double B1, double B2, glm::dvec2 Drop, glm::dvec2 PY) {
		glm::dvec2 PYpos{ 0.0f,0.0f };
		double angle = EdgeVecToCosAngleFloat(PY);
		if (((Drop.x >= A1) && (Drop.x <= A2)) && ((Drop.y >= B1) && (Drop.y <= B2))) {//判断这个点是否在另外一个正方形里面
			if (PY.x > 0) {
				PYpos.x = A2 - Drop.x;
			}
			else {
				PY.x = -PY.x;
				PYpos.x = A1 - Drop.x;
			}

			if (PY.y > 0) {
				PYpos.y = B2 - Drop.y;
			}
			else {
				PY.y = -PY.y;
				PYpos.y = B1 - Drop.y;
			}

			if (PY.x < PY.y) {
				PYpos.x = 0;
			}
			else {
				PYpos.y = 0;
			}
		}
		angle -= EdgeVecToCosAngleFloat(PYpos);
		PYpos /= fabs(cos(angle));
		PYpos = vec2angle(PYpos, angle);
		return PYpos;
	}


	//扭矩计算
	double TorqueCalculate(glm::dvec2 Barycenter, glm::dvec2 Spot, glm::dvec2 Force) {
		glm::vec2 MoveAngle = Barycenter - Spot;
		double AngleA = EdgeVecToCosAngleFloat(MoveAngle);
		double AngleB = EdgeVecToCosAngleFloat(Force);
		double Length = Modulus(MoveAngle);
		return Modulus(Force) * Length * sin(AngleA - AngleB);
	}

	//求解分解力
	[[nodiscard]] DecompositionForce CalculateDecompositionForce(glm::dvec2 angle, glm::dvec2 force) {
		double ObjectAngle = EdgeVecToCosAngleFloat(angle);//获得力臂角度
		double Angle = ObjectAngle - EdgeVecToCosAngleFloat(force);//相差角度
		double Long = SquarePhysics::Modulus(force);//力大小
		double ParallelLong = Long * cos(Angle);//平行力大小
		double VerticalLong = Long * sin(Angle);//垂直力大小
		double CosL = cos(ObjectAngle);
		double SinL = sin(ObjectAngle);
		return { glm::dvec2{ ParallelLong * CosL, ParallelLong * SinL }, glm::dvec2{ VerticalLong * -SinL, VerticalLong * CosL } };//旋转到世界坐标轴
	}

	//计算两个向量的夹角
	[[nodiscard]] double CalculateIncludedAngle(glm::dvec2 V1, glm::dvec2 V2) {
		// 标准化向量
		glm::dvec2 normV1 = glm::normalize(V1);
		glm::dvec2 normV2 = glm::normalize(V2);

		// 计算向量点乘
		double dotProduct = glm::dot(normV1, normV2);

		// 计算夹角（弧度）
		double angleRad = glm::acos(dotProduct);

		// 将弧度转换为角度（可选）
		//double angleDeg = glm::degrees(angleRad);

		return angleRad;
	}

	//计算扭力增加的角速度
	double calculateRotationSpeed(double torque, glm::dvec2 forceToCenter, double mass, double time) {
		// 计算力臂
		double leverArm = glm::length(forceToCenter);  // 受力点到重心的距离

		// 计算角加速度
		double angularAcceleration = torque / (mass * leverArm);

		// 计算下一秒增加的旋转速度
		double newRotationSpeed = angularAcceleration * time;

		return newRotationSpeed;
	}

}