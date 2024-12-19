#pragma once
#include <glm/glm.hpp>
#include "BaseStruct.hpp"

// 移位 > 赋值 > 大小比较 > 加法 > 减法 > 乘法 > 取模 > 除法。

namespace PhysicsBlock
{

	/**
	 * @brief   double 转 int，向下取整
	 * @param   val 需要转换的 double 值
	 * @return  返回转换結果
	 * @note    不是直接丢弃小数点，而是向下取最大的整数*/
	int ToInt(double val);

	/**
	 * @brief   float 轉 int，向下取整
	 * @param   val 需要转换的 float 值
	 * @return  返回转换結果
	 * @note    不是直接丢弃小数点，而是向下取最大的整数*/
	int ToInt(float val);

	/**
	 * @brief   vec2 轉 ivec2，正負都向下取整
	 * @param   val 需要转换的 vec2 值
	 * @return  返回转换結果
	 * @note    不是直接丢弃小数点，而是向下取最大的整数*/
	glm::ivec2 ToInt(glm::vec2 val);

	/**
	 * @brief   dvec2 轉 ivec2，正負都向下取整
	 * @param   val 需要转换的 dvec2 值
	 * @return  返回转换結果
	 * @note    不是直接丢弃小数点，而是向下取最大的整数*/
	glm::ivec2 ToInt(glm::dvec2 val);

	/**
	 * @brief   获取模的长度(没有开方)
	 * @param   Modulus 輸入 X,Y 的距离差
	 * @return  返回 x*x + y*y 的結果 */
	double ModulusLength(glm::dvec2 Modulus);

	/**
	 * @brief   获取模的长度
	 * @param   Modulus 輸入 X,Y 的距离差
	 * @return  返回 (x*x + y*y)開方 的結果 */
	double Modulus(glm::dvec2 Modulus);

	//角度，计算对应的cos, sin 
	glm::dvec2 AngleFloatToAngleVec(double angle);

	/**
	 * @brief 根據 vec2 计算於 X軸正向 的角度
	 * @param XYedge 輸入 X,Y 位置
	 * @return 返回 (x,y) 於 X軸正向 的角度*/
	double EdgeVecToCosAngleFloat(glm::dvec2 XYedge);

	/**
	 * @brief   vec2 基於零坐标旋转
	 * @param   pos 需要旋转的坐标
	 * @param   angle 旋转度数（2 * M_PI 为一圈）
	 * @return  返回旋转結果*/
	glm::dvec2 vec2angle(glm::dvec2 pos, double angle);

	/**
	 * @brief   dvec2 基於零坐标旋转
	 * @param   pos 需要旋转的坐标
	 * @param   angle glm::dvec2 格式 X,Y 存储计算好了的 cos,sin 結果
	 * @return  返回旋转結果*/
	glm::dvec2 vec2angle(glm::dvec2 pos, glm::dvec2 angle);

	/**
	 * @brief   dvec2 基於某個坐标的旋转
	 * @param   pos 需要旋转的点坐标
	 * @param   lingdian 旋转中心的点坐标
	 * @param   angle 旋转度数（2 * M_PI 为一圈）
	 * @return  返回旋转結果*/
	inline glm::dvec2 vec2PosAngle(glm::dvec2 pos, glm::dvec2 lingdian, double angle);

	/**
	 * @brief   dvec2 基於某個坐标的旋转
	 * @param   pos 需要旋转的点坐标
	 * @param   lingdian 旋转中心的点坐标
	 * @param   angle glm::dvec2 格式 X,Y 存储计算好了的 cos,sin 結果
	 * @return  返回旋转結果*/
	inline glm::dvec2 vec2PosAngle(glm::dvec2 pos, glm::dvec2 lingdian, glm::dvec2 angle);

	/**
	 * @brief   坐标点转换坐标系后取整（向下）
	 * @param   Pos 需要转换的点坐标
	 * @param   xPos 兩個坐标系的距离差
	 * @param   angle 兩個坐标系的角度差（2 * M_PI 为一圈）
	 * @return  返回转换后坐标系后的整数坐标*/
	inline glm::ivec2 ToIntPos(glm::dvec2 Pos, glm::dvec2 xPos, double angle);

	/**
	 * @brief   坐标点转换坐标系后取整（向下）
	 * @param   Pos 需要转换的点坐标
	 * @param   xPos 兩個坐标系的距离差
	 * @param   angle 兩個坐标系的角度差（glm::dvec2 格式 X,Y 存储计算好了的 cos,sin 結果）
	 * @return  返回转换后坐标系后的整数坐标*/
	inline glm::ivec2 ToIntPos(glm::dvec2 sPos, glm::dvec2 ePos, glm::dvec2 angle);

	/**
	 * @brief   线段在 x 为某值的焦点
	 * @param   Apos 线段上的任一点 A
	 * @param   Bpos 线段上的任一点 B
	 * @param   x 設置 X 值
	 * @return  返回线段在 X 为某值的 X軸 于线段的焦点
	 * @warning 注意求出的焦点可能不在线段上*/
	glm::dvec2 LineXToPos(glm::dvec2 Apos, glm::dvec2 Bpos, double x);

	/**
	 * @brief   线段在 Y 为某值的焦点
	 * @param   Apos 线段上的任一点 A
	 * @param   Bpos 线段上的任一点 B
	 * @param   y 設置 Y 值
	 * @return  返回线段在 Y 为某值的 Y軸 于线段的焦点
	 * @warning 注意求出的焦点可能不在线段上*/
	glm::dvec2 LineYToPos(glm::dvec2 Apos, glm::dvec2 Bpos, double y);
	
	/**
	 * @brief 裁剪出被矩形覆盖的线段
	 * @param start 起始位置
	 * @param end 结束位置
	 * @param width 宽度
	 * @param height 高度
	 * @return 是否相交，处理后的始终位置
	 * @note 是以原点(0, 0) 和 (width, height) 的矩形
	 * @note LineXToPos 和 LineYToPos 推算得来 */
	SquareFocus LineSquareFocus(glm::dvec2 start, glm::dvec2 end, const double width, const double height);

	/**
	 * @brief 正方形和正方形的碰撞检测（A为静态刚体，B为动态刚体）
	 * @param posA 正方形 A 的位置
	 * @param dA 正方形 A 的边长
	 * @param angleA 正方形 A 的角度
	 * @param posB 正方形 B 的位置
	 * @param dB 正方形 B 的边长
	 * @param angleB 正方形 B 的角度
	 * @return 返回 B 应该偏移的距离 */
	[[nodiscard]] glm::dvec2 SquareToSquare(glm::dvec2 posA, double dA, double angleA, glm::dvec2 posB, double dB, double angleB);

	/**
	 * @brief 矩形和点的碰撞检测
	 * @param A1 矩形最小 x
	 * @param A2 矩形最大 x
	 * @param B1 矩形最小 y
	 * @param B2 矩形最大 y
	 * @param Drop 点的位置坐标
	 * @param PY 期望被移动的方向
	 * @return 返回点应该移动的距离 */
	inline glm::dvec2 SquareToDrop(double A1, double A2, double B1, double B2, glm::dvec2 Drop, glm::dvec2 PY);

	/**
	 * @brief 正方形和点的碰撞检测
	 * @param R 正方形边长的一半
	 * @param Drop 点的位置坐标
	 * @param PY 期望被移动的方向
	 * @return 返回点应该移动的距离 */
	inline glm::dvec2 SquareToDrop(double R, glm::dvec2 Drop, glm::dvec2 PY);

	//正方形和射线的碰撞检测
	glm::dvec2 SquareToRadial(double A1, double A2, double B1, double B2, glm::dvec2 Drop, glm::dvec2 PY);
	
	//扭矩计算
	[[nodiscard]] double TorqueCalculate(glm::dvec2 Barycenter, glm::dvec2 Spot, glm::dvec2 Force);

	//计算夹角
	[[nodiscard]] double CalculateIncludedAngle(glm::dvec2 V1, glm::dvec2 V2);

	/**
	 * @brief 求解分解力
	 * @param angle 力臂
	 * @param force 受力
	 * @return 分解力 */
	DecompositionForce CalculateDecompositionForce(glm::dvec2 angle, glm::dvec2 force);
	DecompositionForceVal CalculateDecompositionForceVal(glm::dvec2 angle, glm::dvec2 force);

	/**
     * @brief 莫顿码
     * @param x x 坐标
     * @param y y 坐标
     * @return 一维索引值
     * @note 让二维索引相邻的一维也尽可能相邻 */
    uint_fast32_t Morton2D(uint_fast16_t x, uint_fast16_t y);

	/**
	 * @brief 点到线段最短距离
	 * @param start 线段起点
	 * @param end 线段终点
	 * @param drop 点位置
	 * @return 垂直最短距离 */
	double DropUptoLineShortes(glm::dvec2 start, glm::dvec2 end, glm::dvec2 drop);

	/**
	 * @brief 点到线段最短距离在线段上的交点
	 * @param start 线段起点
	 * @param end 线段终点
	 * @param drop 点位置
	 * @return 点到线段的垂直交点 */
	glm::dvec2 DropUptoLineShortesIntersect(glm::dvec2 start, glm::dvec2 end, glm::dvec2 drop);
}