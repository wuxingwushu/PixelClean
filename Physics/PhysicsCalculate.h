#pragma once
#include <cmath>
#include "glm/glm.hpp"


// 移位 > 赋值 > 大小比较 > 加法 > 减法 > 乘法 > 取模 > 除法。

namespace SquarePhysics {

	//获取模的长度(没有开方)
	[[nodiscard]] float ModulusLength(glm::vec2 Modulus);
	//获取模的长度
	[[nodiscard]] float Modulus(glm::vec2 Modulus);
	//角度，计算对应的cos, sin 
	[[nodiscard]] glm::vec2 AngleFloatToAngleVec(float angle);
	//根据XY算出cos的角度
	[[nodiscard]] float EdgeVecToCosAngleFloat(glm::vec2 XYedge);

	//vec2旋转(基于原点的旋转)
	[[nodiscard]] glm::dvec2 vec2angle(glm::dvec2 pos, double angle);
	//vec2旋转(基于原点的旋转)
	[[nodiscard]] glm::dvec2 vec2angle(glm::dvec2 pos, glm::dvec2 angle);


	//正方形和正方形的碰撞检测
	[[nodiscard]] glm::dvec2 SquareToSquare(glm::vec2 posA, unsigned int dA, double angleA, glm::vec2 posB, unsigned int dB, double angleB);
	//正方形和点的碰撞检测
	[[nodiscard]] glm::dvec2 SquareToDrop(float A1, float A2, float B1, float B2, glm::dvec2 Drop, glm::dvec2 PY);
	//正方形和射线的碰撞检测
	[[nodiscard]] glm::dvec2 SquareToRadial(float A1, float A2, float B1, float B2, glm::dvec2 Drop, glm::dvec2 PY);
	
	//扭矩计算
	[[nodiscard]] float TorqueCalculate(glm::vec2 Barycenter, glm::vec2 Spot, glm::vec2 Force);
}