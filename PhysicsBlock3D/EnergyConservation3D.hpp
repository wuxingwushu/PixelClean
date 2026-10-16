#pragma once
#include "PhysicsParticle3D.hpp"
#include "PhysicsShape3D.hpp"
#include "BaseCalculate3D.hpp"

// =============================================================================
// EnergyConservation3D —— 2D `PhysicsBlock/EnergyConservation.hpp` 的三维改写
//
// 2D 版提供两个重载：
//   ① EnergyConservation(PhysicsParticle*, PhysicsParticle*)
//      质点-质点理想弹性碰撞（恢复系数 e = 1，不产生角速度）。
//   ② EnergyConservation(PhysicsShape*, PhysicsShape*, Vec2_ CollisionDrop, FLOAT_ Vertical)
//      刚体-刚体弹性碰撞，同时处理平动与转动。
//
// 3D 改写保留同样的两个重载，但实现路线与 2D 有本质区别：
//
//  · 2D 的 ① 先把速度用 acos 分解到"质心连线方向 + 垂直方向"，在法向做一维对心弹性碰撞，
//    再用 sin/cos 合成回世界系。3D 版直接用向量点积取法向分量、用向量加减回填切向分量，
//    数学等价但**不含任何三角函数**，也不会在速度为零时因 `EdgeVecToCosAngleFloat(0)`
//    产生未定义角。
//
//  · 2D 的 ② 是"三次链式角度分解 + 一元二次方程求根"（`EnergyConservationOptimum`
//    的两个分支），且注释自承"假定碰撞点位于两物体质心连线上，此时动能和角动能才同时
//    守恒"。3D 里"质心连线"与"接触法线"一般不一致，且转动惯量是 3×3 张量而非标量，
//    无法照搬标量二次方程。3D 版改为**标准刚体弹性冲量解**：在接触点求相对速度的法向
//    分量，用完整的 3D 有效质量
//        K = invMa + invMb + n·((Ia⁻¹(rA×n))×rA) + n·((Ib⁻¹(rB×n))×rB)
//    求冲量，恢复系数取 1（完全弹性），冲量同时施加平动与转动。
//    这与 `PhysicsBaseArbiter3D` 的求解器同源（那里恢复系数由 bias 项给出），
//    因此 3D 版的能量行为与引擎主求解器自洽，而不是另起一套近似。
//
// 说明：2D 的 `EnergyConservationOptimum` 宏（0=原始二次方程 / 1=优化系数）在 3D 版
//       没有对应物 —— 冲量解法不需要判别式，故不存在"优化与否"的分支。
// =============================================================================

namespace PhysicsBlock3D
{
	/**
	 * @brief 取得向量的单位方向（零向量回退为零向量）
	 * @details 2D 用 `EdgeVecToCosAngleFloat` 求角再 `vec2angle` 还原方向，
	 *          该路径在零向量上未定义。3D 直接归一化。
	 */
	inline Vec3_ DirectionOf3D(const Vec3_ &v)
	{
		const FLOAT_ Length = Modulus(v);
		return (Length > 1e-9f) ? (v / Length) : Vec3_(0.0f);
	}

	/**
	 * @brief 粒子间动能守恒碰撞（质点-质点理想弹性碰撞，完全弹性 e = 1）
	 * @details 把两粒子速度分解到质心连线方向（碰撞法线）与垂直于该方向的切平面；
	 *          法向分量按一维对心弹性碰撞求解（动量守恒 + 动能守恒），
	 *          切向分量不参与碰撞、原样保留。不产生角速度（可理解为光滑球体）。
	 * @param a 粒子A
	 * @param b 粒子B
	 * @note 等价于 2D 版 `EnergyConservation(PhysicsParticle*, PhysicsParticle*)`，
	 *       但用向量投影代替 acos/sin/cos 的角度分解。
	 */
	inline void EnergyConservation3D(PhysicsParticle3D *a, PhysicsParticle3D *b)
	{
		if (a == nullptr || b == nullptr)
		{
			return;
		}

		const Vec3_ Distance = a->pos - b->pos; // 由 B 指向 A
		const FLOAT_ DistanceLength = Modulus(Distance);
		if (DistanceLength <= 1e-9f)
		{
			return; // 两粒子重合，法线方向未定义
		}
		const Vec3_ Normal = Distance / DistanceLength;

		const FLOAT_ Va = Dot(a->speed, Normal); // A 的法向速度分量
		const FLOAT_ Vb = Dot(b->speed, Normal); // B 的法向速度分量

		const FLOAT_ TotalMass = a->mass + b->mass;
		if (TotalMass <= 0.0f)
		{
			return;
		}

		// 一维对心完全弹性碰撞：v' = ((m_self - m_other)·v_self + 2·m_other·v_other) / (m_self + m_other)
		const FLOAT_ NewVa = ((a->mass - b->mass) * Va + 2.0f * b->mass * Vb) / TotalMass;
		const FLOAT_ NewVb = ((b->mass - a->mass) * Vb + 2.0f * a->mass * Va) / TotalMass;

		// 只改法向分量，切向分量自动保留
		a->speed += (NewVa - Va) * Normal;
		b->speed += (NewVb - Vb) * Normal;
	}

	/**
	 * @brief 形状间动能守恒碰撞（刚体-刚体完全弹性碰撞，含角速度）
	 * @details 在接触点求两物体的相对速度，取其沿碰撞法线的分量；
	 *          用完整的三维有效质量（含两体各自的惯量张量贡献）求弹性冲量，
	 *          冲量同时改变平动速度与角速度。
	 * @param a 物体A（刚体）
	 * @param b 物体B（刚体）
	 * @param CollisionDrop 接触点在世界坐标系中的位置
	 * @param Normal 碰撞面法向量（**由物体A指向物体B**，与 `PhysicsBaseArbiter3D`
	 *               的接触法线约定一致；不需要是单位向量，内部会归一化）
	 * @note 与 2D 版不同：不假定接触点位于两质心连线上，且支持各向异性惯量张量。
	 *       本函数是纯弹性（恢复系数 e = 1）的能量守恒碰撞，不做位置修正、不加摩擦。
	 */
	inline void EnergyConservation3D(PhysicsShape3D *a, PhysicsShape3D *b,
									 Vec3_ CollisionDrop, Vec3_ Normal)
	{
		if (a == nullptr || b == nullptr)
		{
			return;
		}

		const Vec3_ n = SafeNormalize(Normal, Vec3_(0.0f, 1.0f, 0.0f));

		const Vec3_ rA = CollisionDrop - a->pos; // A 质心指向接触点
		const Vec3_ rB = CollisionDrop - b->pos; // B 质心指向接触点

		const Mat3_ invIA = a->WorldInvInertia();
		const Mat3_ invIB = b->WorldInvInertia();

		// 接触点处的速度： v = v_质心 + ω × r
		const Vec3_ VelocityA = a->speed + Cross(a->angleSpeed, rA);
		const Vec3_ VelocityB = b->speed + Cross(b->angleSpeed, rB);

		const FLOAT_ Vn = Dot(VelocityB - VelocityA, n);
		if (Vn >= 0.0f)
		{
			return; // 已经在分离，不产生冲量
		}

		// 三维有效质量（与求解器同式）：K = invMa + invMb + n·((Ia⁻¹(rA×n))×rA) + n·((Ib⁻¹(rB×n))×rB)
		const FLOAT_ K = a->invMass + b->invMass +
						 Dot(n, Cross(invIA * Cross(rA, n), rA)) +
						 Dot(n, Cross(invIB * Cross(rB, n), rB));
		if (K <= 1e-12f)
		{
			return; // 双方均不可动
		}

		// 完全弹性：相对法向速度反号，故冲量系数为 -(1 + e)·Vn / K，e = 1
		const FLOAT_ ImpulseMagnitude = -2.0f * Vn / K;
		const Vec3_ Impulse = n * ImpulseMagnitude;

		a->speed -= Impulse * a->invMass;
		a->angleSpeed -= invIA * Cross(rA, Impulse);

		b->speed += Impulse * b->invMass;
		b->angleSpeed += invIB * Cross(rB, Impulse);

		// 2D 的 `OldPosUpDataBool` 防卡机制在此不适用：这是能量守恒碰撞解，
		// 位置修正由 `PhysicsWorld3D` 的 PreStep / 窄相分离量负责。
	}

} // namespace PhysicsBlock3D
