#pragma once
#include "BaseStruct.hpp"
#include <cmath>

// 移位 > 赋值 > 大小比较 > 加法 > 减法 > 乘法 > 取模 > 除法。

namespace PhysicsBlock
{

	/**
	 * @brief   FLOAT_ 转 int，向下取整
	 * @param   val 需要转换的 FLOAT_ 值
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
	 * @brief 快速平方根
	 * @warning 因为现代CPU有开平方根的指令，所以反而会慢于sqrt() */
	float q_sqrt(float S);

	/**
	 * @brief 快速平方根
	 * @warning 因为现代CPU有开平方根的指令，所以反而会慢于sqrt() */
	double q_sqrt(double S);

	/**
	 * @brief   获取模的长度(没有开方)
	 * @param   Modulus 輸入 X,Y 的距离差
	 * @return  返回 x*x + y*y 的結果 */
	FLOAT_ ModulusLength(const Vec2_ &Modulus);

	/**
	 * @brief   获取模的长度
	 * @param   Modulus 輸入 X,Y 的距离差
	 * @return  返回 (x*x + y*y)開方 的結果 */
	FLOAT_ Modulus(const Vec2_ &Modulus);

	// 角度，计算对应的cos, sin
	Vec2_ AngleFloatToAngleVec(FLOAT_ angle);

	/**
	 * @brief 根據 vec2 计算於 X軸正向 的角度
	 * @param XYedge 輸入 X,Y 位置
	 * @return 返回 (x,y) 於 X軸正向 的角度*/
	FLOAT_ EdgeVecToCosAngleFloat(Vec2_ XYedge);

	/**
	 * @brief   vec2 基於零坐标旋转
	 * @param   pos 需要旋转的坐标
	 * @param   angle 旋转度数（2 * M_PI 为一圈）
	 * @return  返回旋转結果*/
	Vec2_ vec2angle(Vec2_ pos, FLOAT_ angle);

	/**
	 * @brief   dvec2 基於零坐标旋转
	 * @param   pos 需要旋转的坐标
	 * @param   angle Vec2_ 格式 X,Y 存储计算好了的 cos,sin 結果
	 * @return  返回旋转結果*/
	Vec2_ vec2angle(Vec2_ pos, Vec2_ angle);

	struct AngleMat
	{
		FLOAT_ Cos;
		FLOAT_ Sin;

		/**
		 * @brief 复用旋转器
		 * @param angle 角度（π）*/
		AngleMat(FLOAT_ angle)
		{
			Cos = cos(angle);
			Sin = sin(angle);
		}

		/**
		 * @brief 设置新旋转角度
		 * @param angle 角度（π）*/
		void SetAngle(FLOAT_ angle)
		{
			Cos = cos(angle);
			Sin = sin(angle);
		}

		/**
		 * @brief 旋转 angle 度（π）
		 * @param pos 位置
		 * @return 旋转结果
		 * @note 绕 （0，0） 旋转 */
		Vec2_ Rotary(Vec2_ pos)
		{
			return Vec2_((pos.x * Cos) - (pos.y * Sin), (pos.x * Sin) + (pos.y * Cos));
		}

		/**
		 * @brief 旋转 -angle 度（π）
		 * @param pos 位置
		 * @return 旋转结果
		 * @note 绕 （0，0） 旋转 */
		Vec2_ Anticlockwise(Vec2_ pos)
		{
			return Vec2_((pos.x * Cos) + (pos.y * Sin), -(pos.x * Sin) + (pos.y * Cos));
		}
	};

	/**
	 * @brief   dvec2 基於某個坐标的旋转
	 * @param   pos 需要旋转的点坐标
	 * @param   lingdian 旋转中心的点坐标
	 * @param   angle 旋转度数（2 * M_PI 为一圈）
	 * @return  返回旋转結果*/
	Vec2_ vec2PosAngle(Vec2_ pos, Vec2_ lingdian, FLOAT_ angle);

	/**
	 * @brief   dvec2 基於某個坐标的旋转
	 * @param   pos 需要旋转的点坐标
	 * @param   lingdian 旋转中心的点坐标
	 * @param   angle Vec2_ 格式 X,Y 存储计算好了的 cos,sin 結果
	 * @return  返回旋转結果*/
	Vec2_ vec2PosAngle(Vec2_ pos, Vec2_ lingdian, Vec2_ angle);

	/**
	 * @brief   坐标点转换坐标系后取整（向下）
	 * @param   Pos 需要转换的点坐标
	 * @param   xPos 兩個坐标系的距离差
	 * @param   angle 兩個坐标系的角度差（2 * M_PI 为一圈）
	 * @return  返回转换后坐标系后的整数坐标*/
	glm::ivec2 ToIntPos(Vec2_ Pos, Vec2_ xPos, FLOAT_ angle);

	/**
	 * @brief   坐标点转换坐标系后取整（向下）
	 * @param   Pos 需要转换的点坐标
	 * @param   xPos 兩個坐标系的距离差
	 * @param   angle 兩個坐标系的角度差（Vec2_ 格式 X,Y 存储计算好了的 cos,sin 結果）
	 * @return  返回转换后坐标系后的整数坐标*/
	glm::ivec2 ToIntPos(Vec2_ sPos, Vec2_ ePos, Vec2_ angle);

	/**
	 * @brief   线段在 x 为某值的焦点
	 * @param   Apos 线段上的任一点 A
	 * @param   Bpos 线段上的任一点 B
	 * @param   x 設置 X 值
	 * @return  返回线段在 X 为某值的 X軸 于线段的焦点
	 * @warning 注意求出的焦点可能不在线段上*/
	Vec2_ LineXToPos(Vec2_ Apos, Vec2_ Bpos, FLOAT_ x);

	/**
	 * @brief   线段在 Y 为某值的焦点
	 * @param   Apos 线段上的任一点 A
	 * @param   Bpos 线段上的任一点 B
	 * @param   y 設置 Y 值
	 * @return  返回线段在 Y 为某值的 Y軸 于线段的焦点
	 * @warning 注意求出的焦点可能不在线段上*/
	Vec2_ LineYToPos(Vec2_ Apos, Vec2_ Bpos, FLOAT_ y);

	/**
	 * @brief 裁剪出被矩形覆盖的线段
	 * @param start 起始位置
	 * @param end 结束位置
	 * @param width 宽度
	 * @param height 高度
	 * @return 是否相交，处理后的始终位置
	 * @note 是以原点(0, 0) 和 (width, height) 的矩形
	 * @note LineXToPos 和 LineYToPos 推算得来 */
	SquareFocus LineSquareFocus(Vec2_ start, Vec2_ end, const FLOAT_ width, const FLOAT_ height);

	/**
	 * @brief 正方形和正方形的碰撞检测（A为静态刚体，B为动态刚体）
	 * @param posA 正方形 A 的位置
	 * @param dA 正方形 A 的边长
	 * @param angleA 正方形 A 的角度
	 * @param posB 正方形 B 的位置
	 * @param dB 正方形 B 的边长
	 * @param angleB 正方形 B 的角度
	 * @return 返回 B 应该偏移的距离 */
	Vec2_ SquareToSquare(Vec2_ posA, FLOAT_ dA, FLOAT_ angleA, Vec2_ posB, FLOAT_ dB, FLOAT_ angleB);

	/**
	 * @brief 矩形和点的碰撞检测
	 * @param A1 矩形最小 x
	 * @param A2 矩形最大 x
	 * @param B1 矩形最小 y
	 * @param B2 矩形最大 y
	 * @param Drop 点的位置坐标
	 * @param PY 期望被移动的方向
	 * @return 返回点应该移动的距离 */
	Vec2_ SquareToDrop(FLOAT_ A1, FLOAT_ A2, FLOAT_ B1, FLOAT_ B2, Vec2_ Drop, Vec2_ PY);

	/**
	 * @brief 正方形和点的碰撞检测
	 * @param R 正方形边长的一半
	 * @param Drop 点的位置坐标
	 * @param PY 期望被移动的方向
	 * @return 返回点应该移动的距离 */
	Vec2_ SquareToDrop(FLOAT_ R, Vec2_ Drop, Vec2_ PY);

	// 正方形和射线的碰撞检测
	Vec2_ SquareToRadial(FLOAT_ A1, FLOAT_ A2, FLOAT_ B1, FLOAT_ B2, Vec2_ Drop, Vec2_ PY);

	// 扭矩计算
	FLOAT_ TorqueCalculate(Vec2_ Barycenter, Vec2_ Spot, Vec2_ Force);

	// 计算夹角
	FLOAT_ CalculateIncludedAngle(Vec2_ V1, Vec2_ V2);

	/**
	 * @brief 求解分解力
	 * @param angle 力臂
	 * @param force 受力
	 * @return 分解力 */
	DecompositionForce CalculateDecompositionForce(Vec2_ angle, Vec2_ force);
	DecompositionForce CalculateDecompositionForce(FLOAT_ angle, Vec2_ force);
	DecompositionForceVal CalculateDecompositionForceVal(Vec2_ angle, Vec2_ force);

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
	FLOAT_ DropUptoLineShortes(Vec2_ start, Vec2_ end, Vec2_ drop);

	/**
	 * @brief 点到线最短距离的交点
	 * @param start 线起点
	 * @param end 线终点
	 * @param drop 点位置
	 * @return 点到线的垂直交点
	 * @note 不重合的两个点 */
	Vec2_ DropLineShortesIntersect(Vec2_ start, Vec2_ end, Vec2_ drop);

	/**
	 * @brief 点到线段的最短距离的交点（在线段上的）
	 * @param start 线起点
	 * @param end 线终点
	 * @param drop 点位置
	 * @return 点到线的垂直交点
	 * @note 不重合的两个点 */
	Vec2_ DropUptoLineShortesIntersect(Vec2_ start, Vec2_ end, Vec2_ drop);

	FLOAT_ Dot(const Vec2_ &a, const Vec2_ &b);
	/**
	 * @brief 投影
	 * @param a
	 * @param b
	 * @return  */
	FLOAT_ Cross(const Vec2_ &a, const Vec2_ &b);

	// 顺时针 转 90度， s 缩放比
	Vec2_ Cross(const Vec2_ &a, FLOAT_ s);

	// 逆时针 转 90度， s 缩放比
	Vec2_ Cross(FLOAT_ s, const Vec2_ &a);

	/**
	 * @brief 返回合理范围内的数
	 * @param a 值
	 * @param low 最低
	 * @param high 最高
	 * @return 合理值
	 * @note a 不合理就返回和他最近的合理值 */
	FLOAT_ Clamp(FLOAT_ a, FLOAT_ low, FLOAT_ high);

	FLOAT_ Random(FLOAT_ lo, FLOAT_ hi);
}