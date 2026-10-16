#pragma once
#include <climits>
#include "BaseStruct3D.hpp"

namespace PhysicsBlock3D
{

	class PhysicsAssembly3D;

	/**
	 * @brief 动态物体的类模板（3D）
	 * @details 由 2D 版 PhysicsFormwork 改写。维度相关的接口变化：
	 *
	 *  1. 位置/速度/力：Vec2_ → Vec3_
	 *  2. 旋转：2D 用标量 `FLOAT_ angle`，3D 改用四元数 `Quat_ rotation`
	 *     （PFGetAngle/PFSetAngle → PFGetRotation/PFSetRotation）。
	 *     标量角在 3D 无法表达绕任意轴的朝向，且欧拉角存在万向节死锁。
	 *  3. 角速度：2D 标量 `FLOAT_ angleSpeed`（绕 Z 轴）→ 3D 向量 `Vec3_ angleSpeed`
	 *     （角速度轴-角形式，方向即旋转轴，模长即角速率）。
	 *  4. 逆转动惯量：2D 标量 `FLOAT_ invMomentInertia` → 3D 张量 `Mat3_ invMomentInertia`
	 *     （3D 转动惯量是 3×3 张量，各轴不同；标量只在 2D 平面旋转下成立）。
	 *  5. 角冲量：2D `FLOAT_` 标量 → 3D `Vec3_` 向量。
	 */
	class PhysicsFormwork3D
	{
	public:
		PhysicsAssembly3D *assembly = nullptr;
		PhysicsObjectEnum type = PhysicsObjectEnum::Null;
		unsigned int mGridIndex = UINT_MAX;

		PhysicsFormwork3D(){};
		virtual ~PhysicsFormwork3D(){};

		/**
		 * @brief 物理外部施加能量对速度的改变
		 * @param time 时间差
		 * @param Ga 重力加速度 */
		virtual void PhysicsSpeed(FLOAT_ time, Vec3_ Ga) { (void)time; (void)Ga; };

		/**
		 * @brief 物理速度对物体的改变
		 * @param time 时间差
		 * @param Ga 重力加速度 */
		virtual void PhysicsPos(FLOAT_ time, Vec3_ Ga) { (void)time; (void)Ga; };

		/**
		 * @brief 获取对象类型
		 * @return 类型 */
		PhysicsObjectEnum PFGetType() { return type; }

		/**
		 * @brief 获取碰撞半径
		 * @return 半径 */
		virtual FLOAT_ PFGetCollisionR() { return 0; }

		/**
		 * @brief 获取位置
		 * @return 位置 */
		virtual Vec3_ PFGetPos() { return Vec3_(0.0f); }

		/**
		 * @brief 获取旋转朝向
		 * @return 四元数朝向；无旋转的派生（如粒子）返回单位四元数
		 * @note 对应 2D 的 PFGetAngle，用于移动组件做朝向平滑插值（3D 用 slerp） */
		virtual Quat_ PFGetRotation() { return IdentityQuat(); }

		/**
		 * @brief 设置旋转朝向
		 * @param rotation 目标朝向四元数
		 * @note 无旋转的派生（如粒子）为 no-op */
		virtual void PFSetRotation(const Quat_ &rotation) { (void)rotation; }

		/**
		 * @brief 获取质量倒数
		 * @return 质量倒数 */
		virtual FLOAT_ PFGetInvMass() { return 0; }

		/**
		 * @brief 获取质量
		 * @return 质量 */
		virtual FLOAT_ PFGetMass() { return 0; }

		/**
		 * @brief 获取逆转动惯量张量
		 * @return 3×3 逆转动惯量张量；无旋转的派生返回零矩阵
		 * @note 对应 2D 的 PFGetInvI（标量） */
		virtual Mat3_ PFGetInvI() { return Mat3_(0.0f); }

		/**
		 * @brief 速度
		 * @return 速度 */
		virtual Vec3_ &PFSpeed() = 0;

		/**
		 * @brief 角速度
		 * @return 角速度（轴-角形式向量）
		 * @note 对应 2D 的 PFAngleSpeed（标量） */
		virtual Vec3_ &PFAngleSpeed() = 0;

		/**
		 * @brief 施加质心力（仅影响平移）
		 * @param Force 要施加的力向量
		 * @note 带默认空实现，地图/运动学等不响应力的派生可保持空实现 */
		virtual void AddForce(Vec3_ Force) { (void)Force; }

		/**
		 * @brief 施加带受力点的力（同时影响平移和旋转）
		 * @param Pos 受力点（世界坐标）
		 * @param Force 力向量
		 * @note 3D 的力矩为 `r × F`（叉积向量），非 2D 的标量 `r.x*F.y - r.y*F.x` */
		virtual void AddForce(Vec3_ Pos, Vec3_ Force)
		{
			(void)Pos;
			AddForce(Force);
		}

		/**
		 * @brief 施加质心冲量（瞬时改变速度：Δv = invMass * impulse）
		 * @param impulse 冲量向量
		 * @note 用于击退、跳跃、爆炸击飞等"瞬时"动作；静态/运动学体（invMass==0）静默忽略 */
		virtual void ApplyImpulse(const Vec3_ &impulse) { (void)impulse; }

		/**
		 * @brief 施加带受力点的冲量（同时产生角速度）
		 * @param impulse 冲量向量
		 * @param worldPoint 受力点（世界坐标）
		 * @note 用于带自旋的击退；平动体退化为质心冲量。
		 *       3D 角速度增量为 `invI * (r × impulse)`（张量乘向量） */
		virtual void ApplyImpulse(const Vec3_ &impulse, const Vec3_ &worldPoint)
		{
			(void)worldPoint;
			ApplyImpulse(impulse);
		}

		/**
		 * @brief 施加纯角冲量（瞬时改变角速度：Δω = invI * torqueImpulse）
		 * @param torqueImpulse 角冲量向量
		 * @note 平动体无旋转，默认 no-op */
		virtual void ApplyTorqueImpulse(const Vec3_ &torqueImpulse) { (void)torqueImpulse; }
	};

}
