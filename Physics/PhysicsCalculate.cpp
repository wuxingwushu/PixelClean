#include "PhysicsCalculate.h"

namespace SquarePhysics {

	//获取模的长度(没有开方)
	float ModulusLength(glm::vec2 Modulus) {
		return ((Modulus.x * Modulus.x) + (Modulus.y * Modulus.y));
	}

	//获取模的长度
	float Modulus(glm::vec2 Modulus) {
		return sqrt((Modulus.x * Modulus.x) + (Modulus.y * Modulus.y));
	}

	//角度，计算对应的cos, sin 
	glm::vec2 AngleFloatToAngleVec(float angle) {
		return { cos(angle), sin(angle) };
	}

	//根据XY算出cos的角度
	float EdgeVecToCosAngleFloat(glm::vec2 XYedge) {
		float BeveledEdge = Modulus(XYedge);
		if (BeveledEdge == 0) {
			return 0;//分母不可以为零
		}
		BeveledEdge = acos(XYedge.x / BeveledEdge);
		if (XYedge.y < 0)
		{
			return -BeveledEdge;
		}
		return BeveledEdge;
	}

	//vec2旋转
	glm::dvec2 vec2angle(glm::dvec2 pos, double angle) {
		double cosangle = cos(angle);
		double sinangle = sin(angle);
		return glm::dvec2((pos.x * cosangle) - (pos.y * sinangle), (pos.x * sinangle) + (pos.y * cosangle));
	}

	//vec2旋转
	glm::dvec2 vec2angle(glm::dvec2 pos, glm::dvec2 angle) {
		return glm::dvec2((pos.x * angle.x) - (pos.y * angle.y), (pos.x * angle.y) + (pos.y * angle.x));
	}

	//正方形和正方形的碰撞检测（A为静态刚体，B为动态刚体）
	glm::dvec2 SquareToSquare(glm::vec2 posA, unsigned int dA, double angleA, glm::vec2 posB, unsigned int dB, double angleB) {
		double rA = double(dA) / 2;//边长的一半
		double rB = double(dB) / 2;
		glm::dvec2 Pos{ posB.x - posA.x, posB.y - posA.y };//两个正方形的  X  Y  的间距
		double R = sqrt(pow(Pos.x, 2) + pow(Pos.y, 2));//两个正方形的距离
		if (R > ((rA + rB) * 1.42f)) {
			return glm::dvec2(0, 0);
		}
		double Angle = angleB - angleA;//两个正方形的角度差
		double cosangle = cos(Angle);
		double sinangle = sin(Angle);


		double A1 = -rA;
		double A2 = +rA;
		double B1 = -rA;
		double B2 = +rA;
		double Acosangle = cos(angleA);
		double Asinangle = sin(angleA);
		double Bcosangle = cos(angleB);
		double Bsinangle = sin(angleB);

		glm::dvec2 QPYpos{ 0.0f,0.0f };//补偿距离
		glm::dvec2 QPos = vec2angle(Pos, { Acosangle, -Asinangle });//以 A 为坐标系，B 正方形的四个点进行判断是否在内
		QPYpos += SquareToDrop(A1, A2, B1, B2, (QPos + QPYpos + vec2angle(glm::vec2(-rB, -rB), { cosangle, sinangle })), QPos);
		QPYpos += SquareToDrop(A1, A2, B1, B2, (QPos + QPYpos + vec2angle(glm::vec2(+rB, -rB), { cosangle, sinangle })), QPos);
		QPYpos += SquareToDrop(A1, A2, B1, B2, (QPos + QPYpos + vec2angle(glm::vec2(-rB, +rB), { cosangle, sinangle })), QPos);
		QPYpos += SquareToDrop(A1, A2, B1, B2, (QPos + QPYpos + vec2angle(glm::vec2(+rB, +rB), { cosangle, sinangle })), QPos);

		QPYpos = vec2angle(QPYpos, { Acosangle, Asinangle });

		A1 = -rB;
		A2 = +rB;
		B1 = -rB;
		B2 = +rB;
		sinangle = -sinangle;

		glm::dvec2 HPYpos{ 0.0f,0.0f };//补偿距离
		glm::dvec2 HPos = vec2angle(-(QPYpos + Pos), { Bcosangle, -Bsinangle });//以 B 为坐标系，A 正方形的四个点进行判断是否在内
		HPYpos -= SquareToDrop(A1, A2, B1, B2, (HPos + HPYpos + vec2angle(glm::vec2(-rA, -rA), { cosangle, sinangle })), HPos);
		HPYpos -= SquareToDrop(A1, A2, B1, B2, (HPos + HPYpos + vec2angle(glm::vec2(+rA, -rA), { cosangle, sinangle })), HPos);
		HPYpos -= SquareToDrop(A1, A2, B1, B2, (HPos + HPYpos + vec2angle(glm::vec2(-rA, +rA), { cosangle, sinangle })), HPos);
		HPYpos -= SquareToDrop(A1, A2, B1, B2, (HPos + HPYpos + vec2angle(glm::vec2(+rA, +rA), { cosangle, sinangle })), HPos);

		return QPYpos + vec2angle(HPYpos, { Bcosangle, Bsinangle });//返回需要矫正的距离
	}

	//正方形和点的碰撞检测
	glm::dvec2 SquareToDrop(float A1, float A2, float B1, float B2, glm::dvec2 Drop, glm::dvec2 PY) {
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

	glm::dvec2 SquareToRadial(float A1, float A2, float B1, float B2, glm::dvec2 Drop, glm::dvec2 PY) {
		glm::dvec2 PYpos{ 0.0f,0.0f };
		float angle = EdgeVecToCosAngleFloat(PY);
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
	float TorqueCalculate(glm::vec2 Barycenter, glm::vec2 Spot, glm::vec2 Force) {
		glm::vec2 MoveAngle = Barycenter - Spot;
		float AngleA = EdgeVecToCosAngleFloat(MoveAngle);
		float AngleB = EdgeVecToCosAngleFloat(Force);
		float Length = Modulus(MoveAngle);
		return Modulus(Force) * Length * sin(AngleA - AngleB);
	}

}