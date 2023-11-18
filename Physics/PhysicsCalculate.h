#pragma once
#include <cmath>
#include "glm/glm.hpp"
#include "StructuralComponents.h"


// 移位 > 赋值 > 大小比较 > 加法 > 减法 > 乘法 > 取模 > 除法。

namespace SquarePhysics {

	//获取模的长度(没有开方)
	float ModulusLength(glm::vec2 Modulus);

	//获取模的长度
	inline float Modulus(glm::vec2 Modulus);

	//角度，计算对应的cos, sin 
	glm::vec2 AngleFloatToAngleVec(float angle);

	//根据XY算出cos的角度
	inline float EdgeVecToCosAngleFloat(glm::vec2 XYedge);

	//vec2旋转(基于原点的旋转)
	inline glm::dvec2 vec2angle(glm::dvec2 pos, double angle);
	//vec2旋转(基于原点的旋转)
	inline glm::dvec2 vec2angle(glm::dvec2 pos, glm::dvec2 angle);


	//正方形和正方形的碰撞检测
	[[nodiscard]] glm::dvec2 SquareToSquare(glm::vec2 posA, unsigned int dA, double angleA, glm::vec2 posB, unsigned int dB, double angleB);

	//正方形和点的碰撞检测
	inline glm::dvec2 SquareToDrop(float A1, float A2, float B1, float B2, glm::dvec2 Drop, glm::dvec2 PY);

	//正方形和射线的碰撞检测
	glm::dvec2 SquareToRadial(float A1, float A2, float B1, float B2, glm::dvec2 Drop, glm::dvec2 PY);
	
	//扭矩计算
	[[nodiscard]] float TorqueCalculate(glm::vec2 Barycenter, glm::vec2 Spot, glm::vec2 Force);

	//求解分解力
	[[nodiscard]] DecompositionForce CalculateDecompositionForce(glm::vec2 angle, glm::vec2 force);

	//计算夹角
	[[nodiscard]] float CalculateIncludedAngle(glm::vec2 V1, glm::vec2 V2);

	//计算扭力增加的角速度
	float calculateRotationSpeed(float torque, glm::vec2 forceToCenter, float mass, float time);
}