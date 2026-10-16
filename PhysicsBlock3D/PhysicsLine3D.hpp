#pragma once
#include "PhysicsFormwork3D.hpp"
#include "PhysicsAngle3D.hpp"

namespace PhysicsBlock3D
{

	/**
	 * @brief 物理线段/胶囊类（3D）
	 * @details 表示一个具有物理属性的线段（胶囊体），继承自 PhysicsAngle3D
	 *          用于模拟线条状物体的物理行为
	 *
	 * @note 由 2D 版 PhysicsLine 改写。2D 的线段是零厚度线段，
	 *       `radius` 字段同时充当"半长度"（构造末尾 `radius /= 2`）。
	 *       3D 明确拆成两个概念：
	 *        - `HalfLength`：沿局部 X 轴的半长度（线段端点 = pos ± 朝向*HalfLength）
	 *        - `radius`：胶囊厚度（碰撞半径的一部分）
	 *       为保持 2D 移植时的行为一致，`radius` 仍按 2D 语义保存**半长度**
	 *       （PFGetCollisionR 依赖它做宽相剔除），厚度另存 `Thickness`。
	 *
	 * 转动惯量（胶囊，绕质心，轴向为局部 X）：
	 *   Ixx = ½·m·r²
	 *   Iyy = Izz = (1/12)·m·(3r² + L²)
	 * 2D 版是细杆公式 mL²/12（标量）。3D 若直接照搬细杆，Ixx 会退化为 0
	 * 导致张量奇异无法求逆；胶囊公式天然正定。
	 */
	class PhysicsLine3D : public PhysicsAngle3D
	{
#if PhysicsBlock_Serialization3D
	public:
		/** @brief 从JSON反序列化构造 */
		PhysicsLine3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data) : PhysicsAngle3D(data)
		{
			type = PhysicsObjectEnum::line;
		};
#endif
	public:
		/**
		 * @brief 半长度（沿局部 X 轴）
		 */
		FLOAT_ HalfLength = 0.0f;

		/**
		 * @brief 胶囊厚度（横截面半径）
		 */
		FLOAT_ Thickness = 0.0f;

		/**
		 * @brief 构造物理线段
		 * @param begin_ 线段起点（世界坐标）
		 * @param end_ 线段终点（世界坐标）
		 * @param mass_ 质量
		 * @param friction_ 摩擦系数，默认为0.2
		 * @param thickness_ 胶囊厚度，默认为0（零厚度线段）
		 */
		PhysicsLine3D(Vec3_ begin_, Vec3_ end_, FLOAT_ mass_, FLOAT_ friction_ = 0.2,
		              FLOAT_ thickness_ = 0.0f);

		/** @brief 析构函数 */
		virtual ~PhysicsLine3D(){};

		/**
		 * @brief 取线段的两个端点（世界坐标）
		 * @param outBegin 起点
		 * @param outEnd 终点
		 * @details 局部 X 轴按当前朝向旋转后，从质心两侧各偏移 HalfLength
		 */
		void GetSegment(Vec3_ &outBegin, Vec3_ &outEnd) const;
	};

}
