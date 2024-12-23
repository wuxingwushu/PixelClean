#pragma once
#include "glm/glm.hpp"
#include "StructuralComponents.h"


// 移位 > 赋值 > 大小比较 > 加法 > 减法 > 乘法 > 取模 > 除法。

namespace SquarePhysics {

	/**
	 * @brief   double 轉 int，向下取整
	 * @param   val 需要轉換的 double 值
	 * @return  返回轉換結果
	 * @note    不是直接丟棄小數點，而是向下取最大的整數*/
	int ToInt(double val);

	/**
	 * @brief   float 轉 int，向下取整
	 * @param   val 需要轉換的 float 值
	 * @return  返回轉換結果
	 * @note    不是直接丟棄小數點，而是向下取最大的整數*/
	int ToInt(float val);

	/**
	 * @brief   vec2 轉 ivec2，正負都向下取整
	 * @param   val 需要轉換的 vec2 值
	 * @return  返回轉換結果
	 * @note    不是直接丟棄小數點，而是向下取最大的整數*/
	glm::ivec2 ToIvec2(glm::vec2 val);

	/**
	 * @brief   dvec2 轉 ivec2，正負都向下取整
	 * @param   val 需要轉換的 dvec2 值
	 * @return  返回轉換結果
	 * @note    不是直接丟棄小數點，而是向下取最大的整數*/
	glm::ivec2 ToIvec2(glm::dvec2 val);

	/**
	 * @brief   获取模的长度(没有开方)
	 * @param   Modulus 輸入 X,Y 的距離差
	 * @return  返回 x*x + y*y 的結果 */
	double ModulusLength(glm::dvec2 Modulus);

	/**
	 * @brief   获取模的长度
	 * @param   Modulus 輸入 X,Y 的距離差
	 * @return  返回 (x*x + y*y)開方 的結果 */
	double Modulus(glm::dvec2 Modulus);

	//角度，计算对应的cos, sin 
	glm::dvec2 AngleFloatToAngleVec(double angle);

	/**
	 * @brief 根據 vec2 計算於 X軸正向 的角度
	 * @param XYedge 輸入 X,Y 位置
	 * @return 返回 (x,y) 於 X軸正向 的角度*/
	double EdgeVecToCosAngleFloat(glm::dvec2 XYedge);

	/**
	 * @brief   vec2 基於零坐標旋轉
	 * @param   pos 需要旋轉的坐標
	 * @param   angle 旋轉度數（2 * M_PI 為一圈）
	 * @return  返回旋轉結果*/
	glm::dvec2 vec2angle(glm::dvec2 pos, double angle);

	/**
	 * @brief   dvec2 基於零坐標旋轉
	 * @param   pos 需要旋轉的坐標
	 * @param   angle glm::dvec2 格式 X,Y 儲存計算好了的 cos,sin 結果
	 * @return  返回旋轉結果*/
	glm::dvec2 vec2angle(glm::dvec2 pos, glm::dvec2 angle);

	/**
	 * @brief   dvec2 基於某個坐標的旋轉
	 * @param   pos 需要旋轉的點坐標
	 * @param   lingdian 旋轉中心的點坐標
	 * @param   angle 旋轉度數（2 * M_PI 為一圈）
	 * @return  返回旋轉結果*/
	glm::dvec2 vec2PosAngle(glm::dvec2 pos, glm::dvec2 lingdian, double angle);

	/**
	 * @brief   dvec2 基於某個坐標的旋轉
	 * @param   pos 需要旋轉的點坐標
	 * @param   lingdian 旋轉中心的點坐標
	 * @param   angle glm::dvec2 格式 X,Y 儲存計算好了的 cos,sin 結果
	 * @return  返回旋轉結果*/
	glm::dvec2 vec2PosAngle(glm::dvec2 pos, glm::dvec2 lingdian, glm::dvec2 angle);

	/**
	 * @brief   坐標點轉換坐標系後取整（向下）
	 * @param   Pos 需要轉換的點坐標
	 * @param   xPos 兩個坐標系的距離差
	 * @param   angle 兩個坐標系的角度差（2 * M_PI 為一圈）
	 * @return  返回轉換後坐標系後的整數坐標*/
	glm::ivec2 ToIntPos(glm::dvec2 Pos, glm::dvec2 xPos, double angle);

	/**
	 * @brief   坐標點轉換坐標系後取整（向下）
	 * @param   Pos 需要轉換的點坐標
	 * @param   xPos 兩個坐標系的距離差
	 * @param   angle 兩個坐標系的角度差（glm::dvec2 格式 X,Y 儲存計算好了的 cos,sin 結果）
	 * @return  返回轉換後坐標系後的整數坐標*/
	glm::ivec2 ToIntPos(glm::dvec2 sPos, glm::dvec2 ePos, glm::dvec2 angle);

	/**
	 * @brief   線段在 x 為某值的焦點
	 * @param   Apos 線段上的任一點 A
	 * @param   Bpos 線段上的任一點 B
	 * @param   x 設置 X 值
	 * @return  返回線段在 X 為某值的 X軸 於線段的焦點
	 * @warning 注意求出的焦點可能不在線段上*/
	glm::dvec2 LineXToPos(glm::dvec2 Apos, glm::dvec2 Bpos, double x);

	/**
	 * @brief   線段在 Y 為某值的焦點
	 * @param   Apos 線段上的任一點 A
	 * @param   Bpos 線段上的任一點 B
	 * @param   y 設置 Y 值
	 * @return  返回線段在 Y 為某值的 Y軸 於線段的焦點
	 * @warning 注意求出的焦點可能不在線段上*/
	glm::dvec2 LineYToPos(glm::dvec2 Apos, glm::dvec2 Bpos, double y);


	/**
	 * @brief 正方形和正方形的碰撞检测（A为静态刚体，B为动态刚体）
	 * @param posA 正方形 A 的位置
	 * @param dA 正方形 A 的邊長
	 * @param angleA 正方形 A 的角度
	 * @param posB 正方形 B 的位置
	 * @param dB 正方形 B 的邊長
	 * @param angleB 正方形 B 的角度
	 * @return 返回 B 應該偏移的距離 */
	glm::dvec2 SquareToSquare(glm::dvec2 posA, unsigned int dA, double angleA, glm::dvec2 posB, unsigned int dB, double angleB);

	/**
	 * @brief 矩形和点的碰撞检测
	 * @param A1 矩形最小 x
	 * @param A2 矩形最大 x
	 * @param B1 矩形最小 y
	 * @param B2 矩形最大 y
	 * @param Drop 點的位置坐標
	 * @param PY 期望被移動的方向
	 * @return 返回點應該移動的距離 */
	glm::dvec2 SquareToDrop(double A1, double A2, double B1, double B2, glm::dvec2 Drop, glm::dvec2 PY);

	/**
	 * @brief 正方形和点的碰撞检测
	 * @param R 正方形邊長的一半
	 * @param Drop 點的位置坐標
	 * @param PY 期望被移動的方向
	 * @return 返回點應該移動的距離 */
	glm::dvec2 SquareToDrop(double R, glm::dvec2 Drop, glm::dvec2 PY);

	//正方形和射线的碰撞检测
	glm::dvec2 SquareToRadial(double A1, double A2, double B1, double B2, glm::dvec2 Drop, glm::dvec2 PY);
	
	//扭矩计算
	double TorqueCalculate(glm::dvec2 Barycenter, glm::dvec2 Spot, glm::dvec2 Force);

	//求解分解力
	DecompositionForce CalculateDecompositionForce(glm::dvec2 angle, glm::dvec2 force);

	//计算夹角
	double CalculateIncludedAngle(glm::dvec2 V1, glm::dvec2 V2);

	//计算扭力增加的角速度
	double calculateRotationSpeed(double torque, glm::dvec2 forceToCenter, double mass, double time);
}