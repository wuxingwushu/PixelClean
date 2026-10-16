#pragma once
#include "PhysicsFormwork3D.hpp"
#include "PhysicsAngle3D.hpp"

namespace PhysicsBlock3D
{

	/**
	 * @brief 物理球类（3D）
	 * @details 继承自 PhysicsAngle3D，表示一个球形的物理对象
	 * 包含位置、半径、质量、摩擦因数等属性
	 *
	 * @note 由 2D 版 PhysicsCircle 改写。名字从 Circle 变为 Sphere 更贴合
	 *       3D 语义，但为便于对照移植，类名保留 `Circle3D` 命名习惯。
	 *
	 * 转动惯量：2D 圆盘 I = ½mr²；3D 实心球 I = (2/5)mr²（各轴相同）。
	 */
	class PhysicsCircle3D : public PhysicsAngle3D
	{
#if PhysicsBlock_Serialization3D
	public:
		/**
		 * @brief 从 JSON 数据构造物理球对象
		 * @param data JSON 数据
		 */
		PhysicsCircle3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data) : PhysicsAngle3D(data)
		{
			type = PhysicsObjectEnum::circle;
		};
#endif
	public:
		/**
		 * @brief 构造函数
		 * @param Pos 球的位置
		 * @param Radius 球的半径
		 * @param Mass 球的质量
		 * @param Friction 摩擦因数，默认为0.2
		 */
		PhysicsCircle3D(Vec3_ Pos, FLOAT_ Radius, FLOAT_ Mass, FLOAT_ Friction = 0.2);

		/**
		 * @brief 析构函数
		 */
		virtual ~PhysicsCircle3D();

		/*=========PhysicsFormwork3D=========*/
	};

}
