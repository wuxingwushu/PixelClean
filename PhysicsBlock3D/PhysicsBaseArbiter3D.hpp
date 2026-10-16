#pragma once
#include "BaseArbiter3D.hpp"
#include "PhysicsAngle3D.hpp"
#include "PhysicsParticle3D.hpp"

namespace PhysicsBlock3D
{
	/** @brief 地图接口前向声明（完整定义见 MapFormwork3D.hpp）
	 *  @details 这里只需指针，故用前向声明解耦，避免对象层依赖地图层。 */
	class MapFormwork3D;

	/**
	 * @brief   动态形状与动态形状的碰撞裁决器（3D）
	 * @details 处理两个 PhysicsAngle3D 对象之间的碰撞响应
	 *
	 * @note 与 2D 求解器的本质差异（有效质量与摩擦求解）：
	 *       2D:  kNormal = invM1 + invM2 + invI1·(R1 − rn1²) + invI2·(R2 − rn2²)
	 *            —— invI 是标量，R1−rn1² 是 |r1×n|² 的展开（2D 叉积标量平方）。
	 *       3D:  kNormal = invM1 + invM2
	 *                    + n·((I1⁻¹(r1×n)) × r1) + n·((I2⁻¹(r2×n)) × r2)
	 *            —— 惯量是 3×3 张量（世界系），必须做共轭变换 R·I⁻¹·Rᵀ。
	 *       摩擦：2D 是单一 tangent = (n.y, −n.x)，冲量在 1D 区间 [−μPn, +μPn] 内钳制；
	 *            3D 需要两个正交切向 t1/t2，摩擦是**圆盘约束** |Pt| ≤ μPn，
	 *            用 ProjectFrictionDisk 做径向投影（而非逐分量钳制，
	 *            逐分量钳制会把圆盘变成正方形，导致摩擦各向异性）。
	 */
	class PhysicsBaseArbiterAA3D : public BaseArbiter3D
	{
	public:
		PhysicsAngle3D *object1; ///< 第一个动态形状对象
		PhysicsAngle3D *object2; ///< 第二个动态形状对象

		/**
		 * @brief   构造函数
		 * @param   Object1 第一个动态形状对象
		 * @param   Object2 第二个动态形状对象
		 */
		PhysicsBaseArbiterAA3D(PhysicsAngle3D *Object1, PhysicsAngle3D *Object2)
		    : BaseArbiter3D(Object1, Object2), object1(Object1), object2(Object2){};

		/** @brief 析构函数 */
		virtual ~PhysicsBaseArbiterAA3D(){};

		/** @brief 更新碰撞信息（继承跨帧累积冲量） */
		virtual void Update(Contact3D *NewContacts, int numNewContacts);

		/** @brief 预处理（计算 r1/r2） */
		virtual void PreStep();

		/** @brief 预处理（计算有效质量/偏差/应用累积冲量） */
		virtual void PreStep(FLOAT_ inv_dt);

		/** @brief 应用冲量（法向 + 摩擦圆盘） */
		virtual void ApplyImpulse();
	};

	/**
	 * @brief   动态形状与动态点的碰撞裁决器（3D）
	 * @details 处理 PhysicsAngle3D 对象与 PhysicsParticle3D 对象之间的碰撞
	 *          点对象没有旋转，故只计算 object1 的转动惯量影响
	 */
	class PhysicsBaseArbiterAD3D : public BaseArbiter3D
	{
	public:
		PhysicsAngle3D *object1;    ///< 动态形状对象
		PhysicsParticle3D *object2; ///< 动态点对象

		/**
		 * @brief   构造函数
		 * @param   Object1 动态形状对象
		 * @param   Object2 动态点对象
		 */
		PhysicsBaseArbiterAD3D(PhysicsAngle3D *Object1, PhysicsParticle3D *Object2)
		    : BaseArbiter3D(Object1, Object2), object1(Object1), object2(Object2){};

		/** @brief 析构函数 */
		virtual ~PhysicsBaseArbiterAD3D(){};

		/** @brief 更新碰撞信息 */
		virtual void Update(Contact3D *NewContacts, int numNewContacts);

		/** @brief 预处理（r2 恒为零向量） */
		virtual void PreStep();

		/** @brief 预处理（只算 object1 的旋转惯量） */
		virtual void PreStep(FLOAT_ inv_dt);

		/** @brief 应用冲量（只更新 object2 的线速度） */
		virtual void ApplyImpulse();
	};

	/**
	 * @brief   动态形状与地图的碰撞裁决器（3D）
	 * @details 地图是静态的，故只更新动态形状的速度
	 */
	class PhysicsBaseArbiterA3D : public BaseArbiter3D
	{
	public:
		PhysicsAngle3D *object1;  ///< 动态形状对象
		MapFormwork3D *object2;   ///< 地图对象（静态）

		/**
		 * @brief   构造函数
		 * @param   Object1 动态形状对象
		 * @param   Object2 地图对象
		 */
		PhysicsBaseArbiterA3D(PhysicsAngle3D *Object1, MapFormwork3D *Object2)
		    : BaseArbiter3D(Object1, Object2), object1(Object1), object2(Object2){};

		/** @brief 析构函数 */
		virtual ~PhysicsBaseArbiterA3D(){};

		/** @brief 更新碰撞信息 */
		virtual void Update(Contact3D *NewContacts, int numNewContacts);

		/** @brief 预处理（r2 恒为零向量） */
		virtual void PreStep();

		/** @brief 预处理（只算 object1 的有效质量） */
		virtual void PreStep(FLOAT_ inv_dt);

		/** @brief 应用冲量（只更新 object1） */
		virtual void ApplyImpulse();
	};

	/**
	 * @brief   动态点与地图的碰撞裁决器（3D）
	 * @details 点没有旋转、地图静态，故只更新点对象的线速度
	 */
	class PhysicsBaseArbiterD3D : public BaseArbiter3D
	{
	public:
		PhysicsParticle3D *object1; ///< 动态点对象
		MapFormwork3D *object2;     ///< 地图对象（静态）

		/**
		 * @brief   构造函数
		 * @param   Object1 动态点对象
		 * @param   Object2 地图对象
		 */
		PhysicsBaseArbiterD3D(PhysicsParticle3D *Object1, MapFormwork3D *Object2)
		    : BaseArbiter3D(Object1, Object2), object1(Object1), object2(Object2){};

		/** @brief 析构函数 */
		virtual ~PhysicsBaseArbiterD3D(){};

		/** @brief 更新碰撞信息 */
		virtual void Update(Contact3D *NewContacts, int numNewContacts);

		/** @brief 预处理（r1 为点到碰撞点向量） */
		virtual void PreStep();

		/** @brief 预处理（无旋转项，切向有效质量 = 法向有效质量） */
		virtual void PreStep(FLOAT_ inv_dt);

		/** @brief 应用冲量（只更新点对象的线速度） */
		virtual void ApplyImpulse();
	};

}
