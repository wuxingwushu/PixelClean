#include "PhysicsBaseArbiter3D.hpp"
#include "BaseCalculate3D.hpp"
#include <algorithm>
#include <utility>

namespace PhysicsBlock3D
{
	/** @brief 位置修正量系数（与 2D 一致） */
	static const FLOAT_ k_biasFactorVAL3D = FLOAT_(0.5);
	/** @brief 允许穿透距离（与 2D 一致） */
	static const FLOAT_ k_allowedPenetration3D = FLOAT_(0.01);

	/**
	 * @brief 位置修正速度上限（m/s）
	 *
	 * 离散碰撞检测下单步穿透可以很深（从 10 m 高落下的方块一步就压进地面 0.17），
	 * 此时 ias = 0.5 * inv_dt * 穿透量 会算出十几米每秒的分离速度，
	 * 求解器会把"压进去"变成"弹出去"，撞得越狠弹得越高，堆叠的塔永远压不实。
	 * 这里给修正速度封顶：深穿透改为分几帧缓慢推出。
	 */
	static const FLOAT_ k_maxBiasVAL3D = FLOAT_(0.2f);

	/**
	 * @brief 接触点热启动：按特征 ID 继承上一帧的累积冲量
	 * @details 2D 用 `w_side` 作为「上一帧 contacts 数组下标」，要求窄相
	 *          返回顺序与上一帧完全一致——一旦流形点数量或顺序变化就会错配。
	 *          3D 窄相（PhysicsBaseCollide3D）用体素坐标 + 面索引哈希生成
	 *          **几何特征 ID**，因此这里改为按特征 ID 匹配：只要两帧命中的是
	 *          同一几何特征，就能正确继承冲量，与返回顺序无关。
	 *          匹配不到的新接触点保持冲量 0（冷启动）。
	 * @param oldContacts 上一帧接触点数组
	 * @param oldCount 上一帧接触点数量
	 * @param newContacts 本帧接触点数组（原地写入继承的冲量）
	 * @param newCount 本帧接触点数量
	 */
	static void WarmStartByFeature(const Contact3D *oldContacts, int oldCount,
	                               Contact3D *newContacts, int newCount)
	{
		for (int i = 0; i < newCount; ++i)
		{
			const unsigned char feature = newContacts[i].w_side;
			for (int j = 0; j < oldCount; ++j)
			{
				if (oldContacts[j].w_side == feature)
				{
					newContacts[i].Pn = oldContacts[j].Pn;
					newContacts[i].Pt1 = oldContacts[j].Pt1;
					newContacts[i].Pt2 = oldContacts[j].Pt2;
					break;
				}
			}
		}
	}

	/**
	 * @brief 把本帧接触点覆盖到持久数组（冲量已由 WarmStartByFeature 填好）
	 */
	static void CommitContacts(Contact3D *contacts, int &numContacts,
	                           const Contact3D *newContacts, int newCount)
	{
		for (int i = 0; i < newCount; ++i)
		{
			contacts[i].normal = newContacts[i].normal;
			contacts[i].position = newContacts[i].position;
			contacts[i].separation = newContacts[i].separation;
			contacts[i].w_side = newContacts[i].w_side;
			contacts[i].Pn = newContacts[i].Pn;
			contacts[i].Pt1 = newContacts[i].Pt1;
			contacts[i].Pt2 = newContacts[i].Pt2;
		}
		numContacts = newCount;
	}

	// ═══════════════════════════════════════════════════════════════
	//  PhysicsBaseArbiterAA3D —— 动态形状 vs 动态形状
	// ═══════════════════════════════════════════════════════════════

	/**
	 * @brief 更新碰撞信息，保留上一帧的累积冲量
	 * @details `w_side` 在 3D 中作为**特征 ID**：新接触点若与旧接触点
	 *          特征相同（同一条棱/同一个面），则继承其 Pn/Pt1/Pt2，
	 *          使冲量在多帧间热启动，收敛更快。
	 */
	void PhysicsBaseArbiterAA3D::Update(Contact3D *NewContacts, int numNewContacts)
	{
		WarmStartByFeature(contacts, numContacts, NewContacts, numNewContacts);
		CommitContacts(contacts, numContacts, NewContacts, numNewContacts);
	}

	/** @brief 预处理（无时间步长）：只算 r1/r2 */
	void PhysicsBaseArbiterAA3D::PreStep()
	{
		for (int i = 0; i < numContacts; ++i)
		{
			Contact3D *c = contacts + i;
			c->r1 = c->position - object1->pos;
			c->r2 = c->position - object2->pos;
		}
	}

	/**
	 * @brief 预处理（带时间步长）：算有效质量、偏差，并应用累积冲量
	 * @details 3D 有效质量（世界系惯量张量）：
	 *          `kNormal = invM1 + invM2 + n·((I1⁻¹(r1×n))×r1) + n·((I2⁻¹(r2×n))×r2)`
	 *          该式等价于 `invM1 + invM2 + (r1×n)ᵀ·I1⁻¹·(r1×n) + ...`，
	 *          是 2D `invI·(R − rn²)` 的三维推广。
	 */
	void PhysicsBaseArbiterAA3D::PreStep(FLOAT_ inv_dt)
	{
		for (int i = 0; i < numContacts; ++i)
		{
			Contact3D *c = contacts + i;
			c->r1 = c->position - object1->pos;
			c->r2 = c->position - object2->pos;

			// 正交切向基（3D 摩擦圆盘需要两个正交切向）
			BuildTangentBasis(c->normal, c->tangent1, c->tangent2);

			const Mat3_ I1inv = object1->WorldInvInertia();
			const Mat3_ I2inv = object2->WorldInvInertia();

			// ── 法向有效质量 ──
			const Vec3_ rn1 = Cross(c->r1, c->normal);
			const Vec3_ rn2 = Cross(c->r2, c->normal);
			FLOAT_ kNormal = object1->invMass + object2->invMass;
			kNormal += Dot(c->normal, Cross(I1inv * rn1, c->r1));
			kNormal += Dot(c->normal, Cross(I2inv * rn2, c->r2));
			c->massNormal = (kNormal > FLOAT_(1e-12)) ? FLOAT_(1.0) / kNormal : FLOAT_(0.0);

			// ── 切向有效质量（两个正交切向分别算）──
			const Vec3_ rt1 = Cross(c->r1, c->tangent1);
			const Vec3_ rt2 = Cross(c->r2, c->tangent1);
			FLOAT_ kT1 = object1->invMass + object2->invMass;
			kT1 += Dot(c->tangent1, Cross(I1inv * rt1, c->r1));
			kT1 += Dot(c->tangent1, Cross(I2inv * rt2, c->r2));
			c->massTangent1 = (kT1 > FLOAT_(1e-12)) ? FLOAT_(1.0) / kT1 : FLOAT_(0.0);

			const Vec3_ ru1 = Cross(c->r1, c->tangent2);
			const Vec3_ ru2 = Cross(c->r2, c->tangent2);
			FLOAT_ kT2 = object1->invMass + object2->invMass;
			kT2 += Dot(c->tangent2, Cross(I1inv * ru1, c->r1));
			kT2 += Dot(c->tangent2, Cross(I2inv * ru2, c->r2));
			c->massTangent2 = (kT2 > FLOAT_(1e-12)) ? FLOAT_(1.0) / kT2 : FLOAT_(0.0);

			// ── 位置修正偏差 ──
			c->bias = -k_biasFactorVAL3D * inv_dt *
			          std::min(FLOAT_(0.0), c->separation + k_allowedPenetration3D);
			c->bias = std::min(c->bias, k_maxBiasVAL3D);

			// ── 应用上一帧累积的冲量（热启动）──
			const Vec3_ P = c->Pn * c->normal + c->Pt1 * c->tangent1 + c->Pt2 * c->tangent2;

			if (object1->invMass != 0)
			{
				object1->speed -= object1->invMass * P;
				object1->angleSpeed -= I1inv * Cross(c->r1, P);
			}
			if (object2->invMass != 0)
			{
				object2->speed += object2->invMass * P;
				object2->angleSpeed += I2inv * Cross(c->r2, P);
			}
		}
	}

	/**
	 * @brief 应用冲量：法向 + 摩擦圆盘
	 * @details 与 2D 的差异：切向冲量不再逐分量钳制到 [−μPn, μPn]
	 *          （那会把摩擦圆盘变成正方形，造成各向异性摩擦），
	 *          而是把 (Pt1, Pt2) 作为二维向量整体投影到半径 μPn 的圆盘内。
	 */
	void PhysicsBaseArbiterAA3D::ApplyImpulse()
	{
		const Mat3_ I1inv = object1->WorldInvInertia();
		const Mat3_ I2inv = object2->WorldInvInertia();

		for (int i = 0; i < numContacts; ++i)
		{
			Contact3D *c = contacts + i;

			// ── 法向 ──
			Vec3_ dv = object2->speed + Cross(object2->angleSpeed, c->r2) -
			           object1->speed - Cross(object1->angleSpeed, c->r1);

			const FLOAT_ vn = Dot(dv, c->normal);
			FLOAT_ dPn = c->massNormal * (-vn + c->bias);

			const FLOAT_ Pn0 = c->Pn;
			c->Pn = std::max(Pn0 + dPn, FLOAT_(0.0));
			dPn = c->Pn - Pn0;

			const Vec3_ Pn = dPn * c->normal;
			if (object1->invMass != 0)
			{
				object1->speed -= object1->invMass * Pn;
				object1->angleSpeed -= I1inv * Cross(c->r1, Pn);
			}
			if (object2->invMass != 0)
			{
				object2->speed += object2->invMass * Pn;
				object2->angleSpeed += I2inv * Cross(c->r2, Pn);
			}

			// ── 摩擦（重算相对速度）──
			dv = object2->speed + Cross(object2->angleSpeed, c->r2) -
			     object1->speed - Cross(object1->angleSpeed, c->r1);

			const FLOAT_ vt1 = Dot(dv, c->tangent1);
			const FLOAT_ vt2 = Dot(dv, c->tangent2);
			const FLOAT_ dPt1 = c->massTangent1 * (-vt1);
			const FLOAT_ dPt2 = c->massTangent2 * (-vt2);

			const FLOAT_ maxPt = c->friction * c->Pn;
			const FLOAT_ old1 = c->Pt1;
			const FLOAT_ old2 = c->Pt2;

			// 圆盘投影（而非逐分量钳制）
			const Vec3_ projected = ProjectFrictionDisk(Vec3_(old1 + dPt1, old2 + dPt2, 0.0f), maxPt);
			c->Pt1 = projected.x;
			c->Pt2 = projected.y;

			const FLOAT_ dPt1a = c->Pt1 - old1;
			const FLOAT_ dPt2a = c->Pt2 - old2;
			const Vec3_ Pt = dPt1a * c->tangent1 + dPt2a * c->tangent2;

			if (object1->invMass != 0)
			{
				object1->speed -= object1->invMass * Pt;
				object1->angleSpeed -= I1inv * Cross(c->r1, Pt);
			}
			if (object2->invMass != 0)
			{
				object2->speed += object2->invMass * Pt;
				object2->angleSpeed += I2inv * Cross(c->r2, Pt);
			}
		}
	}

	// ═══════════════════════════════════════════════════════════════
	//  PhysicsBaseArbiterAD3D —— 动态形状 vs 动态点
	// ═══════════════════════════════════════════════════════════════

	/** @brief 更新碰撞信息（继承累积冲量） */
	void PhysicsBaseArbiterAD3D::Update(Contact3D *NewContacts, int numNewContacts)
	{
		WarmStartByFeature(contacts, numContacts, NewContacts, numNewContacts);
		CommitContacts(contacts, numContacts, NewContacts, numNewContacts);
	}

	/** @brief 预处理：点对象无旋转半径 */
	void PhysicsBaseArbiterAD3D::PreStep()
	{
		for (int i = 0; i < numContacts; ++i)
		{
			Contact3D *c = contacts + i;
			c->r1 = c->position - object1->pos;
			c->r2 = Vec3_(0.0f);
		}
	}

	/** @brief 预处理：只有 object1 有转动惯量 */
	void PhysicsBaseArbiterAD3D::PreStep(FLOAT_ inv_dt)
	{
		const Mat3_ I1inv = object1->WorldInvInertia();

		for (int i = 0; i < numContacts; ++i)
		{
			Contact3D *c = contacts + i;
			c->r1 = c->position - object1->pos;
			c->r2 = Vec3_(0.0f);

			BuildTangentBasis(c->normal, c->tangent1, c->tangent2);

			const Vec3_ rn1 = Cross(c->r1, c->normal);
			FLOAT_ kNormal = object1->invMass + object2->invMass;
			kNormal += Dot(c->normal, Cross(I1inv * rn1, c->r1));
			c->massNormal = (kNormal > FLOAT_(1e-12)) ? FLOAT_(1.0) / kNormal : FLOAT_(0.0);

			const Vec3_ rt1 = Cross(c->r1, c->tangent1);
			FLOAT_ kT1 = object1->invMass + object2->invMass;
			kT1 += Dot(c->tangent1, Cross(I1inv * rt1, c->r1));
			c->massTangent1 = (kT1 > FLOAT_(1e-12)) ? FLOAT_(1.0) / kT1 : FLOAT_(0.0);

			const Vec3_ ru1 = Cross(c->r1, c->tangent2);
			FLOAT_ kT2 = object1->invMass + object2->invMass;
			kT2 += Dot(c->tangent2, Cross(I1inv * ru1, c->r1));
			c->massTangent2 = (kT2 > FLOAT_(1e-12)) ? FLOAT_(1.0) / kT2 : FLOAT_(0.0);

			c->bias = -k_biasFactorVAL3D * inv_dt *
			          std::min(FLOAT_(0.0), c->separation + k_allowedPenetration3D);
			c->bias = std::min(c->bias, k_maxBiasVAL3D);

			const Vec3_ P = c->Pn * c->normal + c->Pt1 * c->tangent1 + c->Pt2 * c->tangent2;

			if (object1->invMass != 0)
			{
				object1->speed -= object1->invMass * P;
				object1->angleSpeed -= I1inv * Cross(c->r1, P);
			}
			if (object2->invMass != 0)
			{
				object2->speed += object2->invMass * P;
			}
		}
	}

	/** @brief 应用冲量：点对象只更新线速度 */
	void PhysicsBaseArbiterAD3D::ApplyImpulse()
	{
		const Mat3_ I1inv = object1->WorldInvInertia();

		for (int i = 0; i < numContacts; ++i)
		{
			Contact3D *c = contacts + i;

			Vec3_ dv = object2->speed - object1->speed - Cross(object1->angleSpeed, c->r1);

			const FLOAT_ vn = Dot(dv, c->normal);
			FLOAT_ dPn = c->massNormal * (-vn + c->bias);

			const FLOAT_ Pn0 = c->Pn;
			c->Pn = std::max(Pn0 + dPn, FLOAT_(0.0));
			dPn = c->Pn - Pn0;

			const Vec3_ Pn = dPn * c->normal;
			if (object1->invMass != 0)
			{
				object1->speed -= object1->invMass * Pn;
				object1->angleSpeed -= I1inv * Cross(c->r1, Pn);
			}
			if (object2->invMass != 0)
			{
				object2->speed += object2->invMass * Pn;
			}

			dv = object2->speed - object1->speed - Cross(object1->angleSpeed, c->r1);

			const FLOAT_ vt1 = Dot(dv, c->tangent1);
			const FLOAT_ vt2 = Dot(dv, c->tangent2);
			const FLOAT_ dPt1 = c->massTangent1 * (-vt1);
			const FLOAT_ dPt2 = c->massTangent2 * (-vt2);

			const FLOAT_ maxPt = c->friction * c->Pn;
			const FLOAT_ old1 = c->Pt1;
			const FLOAT_ old2 = c->Pt2;

			const Vec3_ projected = ProjectFrictionDisk(Vec3_(old1 + dPt1, old2 + dPt2, 0.0f), maxPt);
			c->Pt1 = projected.x;
			c->Pt2 = projected.y;

			const Vec3_ Pt = (c->Pt1 - old1) * c->tangent1 + (c->Pt2 - old2) * c->tangent2;

			if (object1->invMass != 0)
			{
				object1->speed -= object1->invMass * Pt;
				object1->angleSpeed -= I1inv * Cross(c->r1, Pt);
			}
			if (object2->invMass != 0)
			{
				object2->speed += object2->invMass * Pt;
			}
		}
	}

	// ═══════════════════════════════════════════════════════════════
	//  PhysicsBaseArbiterA3D —— 动态形状 vs 地图
	// ═══════════════════════════════════════════════════════════════

	/** @brief 更新碰撞信息（继承累积冲量） */
	void PhysicsBaseArbiterA3D::Update(Contact3D *NewContacts, int numNewContacts)
	{
		WarmStartByFeature(contacts, numContacts, NewContacts, numNewContacts);
		CommitContacts(contacts, numContacts, NewContacts, numNewContacts);
	}

	/** @brief 预处理：地图静态，r2 恒为零 */
	void PhysicsBaseArbiterA3D::PreStep()
	{
		for (int i = 0; i < numContacts; ++i)
		{
			Contact3D *c = contacts + i;
			c->r1 = c->position - object1->pos;
			c->r2 = Vec3_(0.0f);
		}
	}

	/**
	 * @brief 预处理：只算 object1 的有效质量
	 * @note 修正了 2D 版的符号不一致缺陷：2D 的 PreStep 用
	 *       `P = Pn*normal − Pt*tangent`，而 ApplyImpulse 用
	 *       `tangent = Cross(normal, -1.0)`，两者虽等价但极易误读。
	 *       3D 版统一为 `P = Pn*n + Pt1*t1 + Pt2*t2`。
	 */
	void PhysicsBaseArbiterA3D::PreStep(FLOAT_ inv_dt)
	{
		if (object1->invMass == 0)
			return;

		const Mat3_ I1inv = object1->WorldInvInertia();

		for (int i = 0; i < numContacts; ++i)
		{
			Contact3D *c = contacts + i;
			c->r1 = c->position - object1->pos;
			c->r2 = Vec3_(0.0f);

			BuildTangentBasis(c->normal, c->tangent1, c->tangent2);

			const Vec3_ rn1 = Cross(c->r1, c->normal);
			FLOAT_ kNormal = object1->invMass;
			kNormal += Dot(c->normal, Cross(I1inv * rn1, c->r1));
			c->massNormal = (kNormal > FLOAT_(1e-12)) ? FLOAT_(1.0) / kNormal : FLOAT_(0.0);

			const Vec3_ rt1 = Cross(c->r1, c->tangent1);
			FLOAT_ kT1 = object1->invMass;
			kT1 += Dot(c->tangent1, Cross(I1inv * rt1, c->r1));
			c->massTangent1 = (kT1 > FLOAT_(1e-12)) ? FLOAT_(1.0) / kT1 : FLOAT_(0.0);

			const Vec3_ ru1 = Cross(c->r1, c->tangent2);
			FLOAT_ kT2 = object1->invMass;
			kT2 += Dot(c->tangent2, Cross(I1inv * ru1, c->r1));
			c->massTangent2 = (kT2 > FLOAT_(1e-12)) ? FLOAT_(1.0) / kT2 : FLOAT_(0.0);

			c->bias = -k_biasFactorVAL3D * inv_dt *
			          std::min(FLOAT_(0.0), c->separation + k_allowedPenetration3D);
			c->bias = std::min(c->bias, k_maxBiasVAL3D);

			const Vec3_ P = c->Pn * c->normal + c->Pt1 * c->tangent1 + c->Pt2 * c->tangent2;
			object1->speed -= object1->invMass * P;
			object1->angleSpeed -= I1inv * Cross(c->r1, P);
		}
	}

	/** @brief 应用冲量：只更新 object1 */
	void PhysicsBaseArbiterA3D::ApplyImpulse()
	{
		if (object1->invMass == 0)
			return;

		const Mat3_ I1inv = object1->WorldInvInertia();

		for (int i = 0; i < numContacts; ++i)
		{
			Contact3D *c = contacts + i;

			Vec3_ dv = -object1->speed - Cross(object1->angleSpeed, c->r1);

			const FLOAT_ vn = Dot(dv, c->normal);
			FLOAT_ dPn = c->massNormal * (-vn + c->bias);

			const FLOAT_ Pn0 = c->Pn;
			c->Pn = std::max(Pn0 + dPn, FLOAT_(0.0));
			dPn = c->Pn - Pn0;

			const Vec3_ Pn = dPn * c->normal;
			object1->speed -= object1->invMass * Pn;
			object1->angleSpeed -= I1inv * Cross(c->r1, Pn);

			dv = -object1->speed - Cross(object1->angleSpeed, c->r1);

			const FLOAT_ vt1 = Dot(dv, c->tangent1);
			const FLOAT_ vt2 = Dot(dv, c->tangent2);
			const FLOAT_ dPt1 = c->massTangent1 * (-vt1);
			const FLOAT_ dPt2 = c->massTangent2 * (-vt2);

			const FLOAT_ maxPt = c->friction * c->Pn;
			const FLOAT_ old1 = c->Pt1;
			const FLOAT_ old2 = c->Pt2;

			const Vec3_ projected = ProjectFrictionDisk(Vec3_(old1 + dPt1, old2 + dPt2, 0.0f), maxPt);
			c->Pt1 = projected.x;
			c->Pt2 = projected.y;

			const Vec3_ Pt = (c->Pt1 - old1) * c->tangent1 + (c->Pt2 - old2) * c->tangent2;
			object1->speed -= object1->invMass * Pt;
			object1->angleSpeed -= I1inv * Cross(c->r1, Pt);
		}
	}

	// ═══════════════════════════════════════════════════════════════
	//  PhysicsBaseArbiterD3D —— 动态点 vs 地图
	// ═══════════════════════════════════════════════════════════════

	/** @brief 更新碰撞信息（继承累积冲量） */
	void PhysicsBaseArbiterD3D::Update(Contact3D *NewContacts, int numNewContacts)
	{
		WarmStartByFeature(contacts, numContacts, NewContacts, numNewContacts);
		CommitContacts(contacts, numContacts, NewContacts, numNewContacts);
	}

	/** @brief 预处理：点无旋转、地图静态 */
	void PhysicsBaseArbiterD3D::PreStep()
	{
		for (int i = 0; i < numContacts; ++i)
		{
			Contact3D *c = contacts + i;
			c->r1 = c->position - object1->pos;
			c->r2 = Vec3_(0.0f);
		}
	}

	/** @brief 预处理：无旋转项，两个切向有效质量都等于法向有效质量 */
	void PhysicsBaseArbiterD3D::PreStep(FLOAT_ inv_dt)
	{
		if (object1->invMass == 0)
			return;

		for (int i = 0; i < numContacts; ++i)
		{
			Contact3D *c = contacts + i;
			c->r1 = c->position - object1->pos;
			c->r2 = Vec3_(0.0f);

			BuildTangentBasis(c->normal, c->tangent1, c->tangent2);

			const FLOAT_ kNormal = object1->invMass;
			c->massNormal = (kNormal > FLOAT_(1e-12)) ? FLOAT_(1.0) / kNormal : FLOAT_(0.0);
			c->massTangent1 = c->massNormal;
			c->massTangent2 = c->massNormal;

			c->bias = -k_biasFactorVAL3D * inv_dt *
			          std::min(FLOAT_(0.0), c->separation + k_allowedPenetration3D);
			c->bias = std::min(c->bias, k_maxBiasVAL3D);

			const Vec3_ P = c->Pn * c->normal + c->Pt1 * c->tangent1 + c->Pt2 * c->tangent2;
			object1->speed -= object1->invMass * P;
		}
	}

	/** @brief 应用冲量：只更新点对象的线速度 */
	void PhysicsBaseArbiterD3D::ApplyImpulse()
	{
		if (object1->invMass == 0)
			return;

		for (int i = 0; i < numContacts; ++i)
		{
			Contact3D *c = contacts + i;

			Vec3_ dv = -object1->speed;

			const FLOAT_ vn = Dot(dv, c->normal);
			FLOAT_ dPn = c->massNormal * (-vn + c->bias);

			const FLOAT_ Pn0 = c->Pn;
			c->Pn = std::max(Pn0 + dPn, FLOAT_(0.0));
			dPn = c->Pn - Pn0;

			object1->speed -= object1->invMass * (dPn * c->normal);

			dv = -object1->speed;

			const FLOAT_ vt1 = Dot(dv, c->tangent1);
			const FLOAT_ vt2 = Dot(dv, c->tangent2);
			const FLOAT_ dPt1 = c->massTangent1 * (-vt1);
			const FLOAT_ dPt2 = c->massTangent2 * (-vt2);

			const FLOAT_ maxPt = c->friction * c->Pn;
			const FLOAT_ old1 = c->Pt1;
			const FLOAT_ old2 = c->Pt2;

			const Vec3_ projected = ProjectFrictionDisk(Vec3_(old1 + dPt1, old2 + dPt2, 0.0f), maxPt);
			c->Pt1 = projected.x;
			c->Pt2 = projected.y;

			const Vec3_ Pt = (c->Pt1 - old1) * c->tangent1 + (c->Pt2 - old2) * c->tangent2;
			object1->speed -= object1->invMass * Pt;
		}
	}

}
