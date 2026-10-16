#include "PhysicsBaseCollide3D.hpp"
#include "BaseCalculate3D.hpp"
#include <cmath>
#include <algorithm>
#include <cfloat>

namespace PhysicsBlock3D
{

	namespace
	{
		/**
		 * @brief 求"入射面的外向法线"
		 * @details DDA 命中时 `info.Direction` 编码的是命中体素的**入口面**
		 *          （BaseGrid3D::FaceFromStep 保证），`DirectionNormal` 返回该面的
		 *          外向单位法向。对于入射（进入障碍）的射线，外向法线必然与射线
		 *          方向反向；这里用点积做一次自校正，杜绝方向编码在边界情况下的歧义。
		 * @param direction 面方向编码（0..5）
		 * @param rayDir 射线方向（不必归一化）
		 * @return 入射面的外向单位法线（指向射线来向）
		 */
		inline Vec3_ IncidentFaceNormal(unsigned char direction, const Vec3_ &rayDir)
		{
			Vec3_ n = DirectionNormal(direction);
			if (Dot(n, rayDir) > 0.0f)
			{
				n = -n;
			}
			return n;
		}

		/**
		 * @brief 带容量保护的接触点写入
		 */
		inline unsigned int PushContact(Contact3D *contacts, unsigned int count,
		                                const Vec3_ &position, const Vec3_ &normal,
		                                FLOAT_ separation, FLOAT_ friction,
		                                unsigned char featureId)
		{
			if (count >= PhysicsContactMaxSize3D)
			{
				return count;
			}
			Contact3D &c = contacts[count];
			c.position = position;
			c.normal = normal;
			c.separation = separation;
			c.friction = friction;
			c.w_side = featureId;
			return count + 1;
		}
	}

	// =========================================================================
	// 宽相：AABB 粗筛
	// =========================================================================

	bool CollideAABB(PhysicsShape3D *ShapeA, PhysicsShape3D *ShapeB)
	{
		const FLOAT_ R = ShapeA->radius + ShapeB->radius;
		if (std::abs(ShapeA->pos.x - ShapeB->pos.x) > R) return false;
		if (std::abs(ShapeA->pos.y - ShapeB->pos.y) > R) return false;
		if (std::abs(ShapeA->pos.z - ShapeB->pos.z) > R) return false; // 3D 新增
		return true;
	}

	bool CollideAABB(PhysicsShape3D *Shape, PhysicsParticle3D *Particle)
	{
		const FLOAT_ R = Shape->radius; // 点无半径
		if (std::abs(Shape->pos.x - Particle->pos.x) > R) return false;
		if (std::abs(Shape->pos.y - Particle->pos.y) > R) return false;
		if (std::abs(Shape->pos.z - Particle->pos.z) > R) return false; // 3D 新增
		return true;
	}

	bool CollideAABB(PhysicsShape3D *Shape, PhysicsCircle3D *Circle)
	{
		const FLOAT_ R = Shape->radius + Circle->radius;
		if (std::abs(Shape->pos.x - Circle->pos.x) > R) return false;
		if (std::abs(Shape->pos.y - Circle->pos.y) > R) return false;
		if (std::abs(Shape->pos.z - Circle->pos.z) > R) return false; // 3D 新增
		return true;
	}

	bool CollideAABB(PhysicsCircle3D *CircleA, PhysicsCircle3D *CircleB)
	{
		const FLOAT_ R = CircleA->radius + CircleB->radius;
		if (std::abs(CircleA->pos.x - CircleB->pos.x) > R) return false;
		if (std::abs(CircleA->pos.y - CircleB->pos.y) > R) return false;
		if (std::abs(CircleA->pos.z - CircleB->pos.z) > R) return false; // 3D 新增
		return true;
	}

	bool CollideAABB(PhysicsCircle3D *Circle, PhysicsParticle3D *Particle)
	{
		const FLOAT_ R = Circle->radius;
		if (std::abs(Circle->pos.x - Particle->pos.x) > R) return false;
		if (std::abs(Circle->pos.y - Particle->pos.y) > R) return false;
		if (std::abs(Circle->pos.z - Particle->pos.z) > R) return false; // 3D 新增
		return true;
	}

	/**
	 * @brief 线段 vs 形状的宽相
	 * @details 2D 用 `pR = vec2angle({radius,0}, angle)` 取线段轴对齐包围盒的半长。
	 *          3D 改为把局部 X 轴按朝向旋转后取三分量绝对值——这就是线段在
	 *          世界系下的 AABB 半长（对称线段，端点 = pos ± axis）。
	 */
	bool CollideAABB(PhysicsLine3D *Line, PhysicsShape3D *Shape)
	{
		const Vec3_ pR = glm::abs(QuatRotate(Line->rotation, Vec3_(Line->radius, 0.0f, 0.0f)));
		const FLOAT_ R = Shape->radius + Line->Thickness;
		if (std::abs(Line->pos.x - Shape->pos.x) > R + pR.x) return false;
		if (std::abs(Line->pos.y - Shape->pos.y) > R + pR.y) return false;
		if (std::abs(Line->pos.z - Shape->pos.z) > R + pR.z) return false;
		return true;
	}

	bool CollideAABB(PhysicsLine3D *Line, PhysicsCircle3D *Circle)
	{
		const Vec3_ pR = glm::abs(QuatRotate(Line->rotation, Vec3_(Line->radius, 0.0f, 0.0f)));
		const FLOAT_ R = Circle->radius + Line->Thickness;
		if (std::abs(Line->pos.x - Circle->pos.x) > R + pR.x) return false;
		if (std::abs(Line->pos.y - Circle->pos.y) > R + pR.y) return false;
		if (std::abs(Line->pos.z - Circle->pos.z) > R + pR.z) return false;
		return true;
	}

	// =========================================================================
	// 3D 专有几何辅助
	// =========================================================================

	bool ClosestSegmentSegment(const Vec3_ &p1, const Vec3_ &q1,
	                           const Vec3_ &p2, const Vec3_ &q2,
	                           Vec3_ &c1, Vec3_ &c2, FLOAT_ &dist)
	{
		// Ericson, Real-Time Collision Detection, §5.1.9
		const Vec3_ d1 = q1 - p1;
		const Vec3_ d2 = q2 - p2;
		const Vec3_ r = p1 - p2;

		const FLOAT_ a = Dot(d1, d1); // 线段1 长度平方
		const FLOAT_ e = Dot(d2, d2); // 线段2 长度平方
		const FLOAT_ f = Dot(d2, r);

		FLOAT_ s = 0.0f;
		FLOAT_ t = 0.0f;

		const FLOAT_ kEps = 1e-9f;

		if (a <= kEps && e <= kEps)
		{
			// 两线段都退化为点
			c1 = p1;
			c2 = p2;
			dist = Modulus(c1 - c2);
			return dist <= kEps;
		}

		if (a <= kEps)
		{
			// 线段1 退化为点
			s = 0.0f;
			t = Clamp(f / e, 0.0f, 1.0f);
		}
		else
		{
			const FLOAT_ c = Dot(d1, r);
			if (e <= kEps)
			{
				// 线段2 退化为点
				t = 0.0f;
				s = Clamp(-c / a, 0.0f, 1.0f);
			}
			else
			{
				const FLOAT_ b = Dot(d1, d2);
				const FLOAT_ denom = a * e - b * b; // ≥ 0
				if (denom > kEps)
				{
					s = Clamp((b * f - c * e) / denom, 0.0f, 1.0f);
				}
				else
				{
					// 两线段平行：任取 s=0 求最优 t
					s = 0.0f;
				}
				t = (b * s + f) / e;
				if (t < 0.0f)
				{
					t = 0.0f;
					s = Clamp(-c / a, 0.0f, 1.0f);
				}
				else if (t > 1.0f)
				{
					t = 1.0f;
					s = Clamp((b - c) / a, 0.0f, 1.0f);
				}
			}
		}

		c1 = p1 + d1 * s;
		c2 = p2 + d2 * t;
		dist = Modulus(c1 - c2);
		return dist <= kEps;
	}

	void ReduceManifold(Contact3D *contacts, unsigned int &count)
	{
		if (count <= PhysicsContactManifoldMaxSize3D)
		{
			// 点数已在流形上限内，直接保留各点的几何特征 ID（w_side）。
			// 注意：**不要**把 w_side 重编号为数组下标——那会抹掉几何特征，
			// 使 PhysicsBaseArbiter3D 的热启动无法按特征匹配上一帧冲量。
			return;
		}

		// 1) 按穿透深度排序（separation 越小 = 越深）
		std::sort(contacts, contacts + count,
		          [](const Contact3D &a, const Contact3D &b) {
			          return a.separation < b.separation;
		          });

		// 2) 贪心最远点采样：先取最深点，之后每次取"离已选点集最远"的点，
		//    保证流形铺开在接触面上而不是挤在一角。
		Contact3D chosen[PhysicsContactManifoldMaxSize3D];
		bool used[PhysicsContactMaxSize3D] = {false};

		int n = 0;
		chosen[n] = contacts[0];
		used[0] = true;
		++n;

		while (n < static_cast<int>(PhysicsContactManifoldMaxSize3D))
		{
			int best = -1;
			FLOAT_ bestMinDist = -1.0f;
			for (unsigned int i = 0; i < count; ++i)
			{
				if (used[i]) continue;
				FLOAT_ minDist = FLT_MAX;
				for (int j = 0; j < n; ++j)
				{
					const FLOAT_ d = Modulus(contacts[i].position - chosen[j].position);
					if (d < minDist) minDist = d;
				}
				if (minDist > bestMinDist)
				{
					bestMinDist = minDist;
					best = static_cast<int>(i);
				}
			}
			if (best < 0) break;
			used[best] = true;
			chosen[n] = contacts[best];
			++n;
		}

		for (int i = 0; i < n; ++i)
		{
			// 同样保留 chosen[i].w_side（拷贝自原接触点），不重编号
			contacts[i] = chosen[i];
		}
		count = static_cast<unsigned int>(n);
	}

	unsigned char FeatureId(IVec3_ voxel, unsigned int faceIndex)
	{
		// 空间哈希（Knuth 乘法散列），压缩到 [0, PhysicsContactMaxSize3D)
		unsigned int h = static_cast<unsigned int>(voxel.x) * 73856093u;
		h ^= static_cast<unsigned int>(voxel.y) * 19349663u;
		h ^= static_cast<unsigned int>(voxel.z) * 83492791u;
		h ^= faceIndex * 2654435761u;
		h ^= h >> 13;
		return static_cast<unsigned char>(h % PhysicsContactMaxSize3D);
	}

	// =========================================================================
	// 窄相实现
	// =========================================================================

	/**
	 * @brief 形状 vs 形状
	 * @details 双向轮廓面射线检测。法线由 DDA 命中面的外向法线几何确定：
	 *   · 第一轮：从 A 的内部种子点射向 A 的表面点，命中 B 的入射面。
	 *     该面外向法线指向 B→A，故 normal(A→B) = −n。
	 *   · 第二轮：对调 A/B，命中 A 的入射面，外向法线即 A→B，故 normal = +n。
	 */
	unsigned int Collide(Contact3D *contacts, PhysicsShape3D *ShapeA, PhysicsShape3D *ShapeB)
	{
		unsigned int count = 0;

		// ===== 第一轮：ShapeA 轮廓面 → ShapeB =====
		for (size_t i = 0; i < ShapeA->OutlineSet.size() && count < PhysicsContactMaxSize3D; ++i)
		{
			const OutlinePoint3D &p = ShapeA->OutlineSet[i];

			const Vec3_ dropLocal = p.LocalPos;
			const Vec3_ dropPos = ShapeA->pos + QuatRotate(ShapeA->rotation, dropLocal);

			const Vec3_ seedWorld = ShapeA->OldPos + QuatRotate(ShapeA->rotation, p.SeedPos);

			const CollisionInfoD3D info = ShapeB->PsDDADetection(seedWorld, dropPos);
			if (!info.Collision) continue;

			const Vec3_ rayDir = dropPos - seedWorld;
			const Vec3_ n = IncidentFaceNormal(info.Direction, rayDir);

			const Vec3_ normal = -n; // 由 object1(A) 指向 object2(B)
			const FLOAT_ separation = -std::abs(Dot(dropPos - info.pos, normal));
			const FLOAT_ friction = SQRT_(info.Friction * p.Friction);

			count = PushContact(contacts, count, info.pos, normal, separation, friction,
			                    FeatureId(p.Voxel, 0));
		}

		// ===== 第二轮：ShapeB 轮廓面 → ShapeA =====
		for (size_t i = 0; i < ShapeB->OutlineSet.size() && count < PhysicsContactMaxSize3D; ++i)
		{
			const OutlinePoint3D &p = ShapeB->OutlineSet[i];

			const Vec3_ dropPos = ShapeB->pos + QuatRotate(ShapeB->rotation, p.LocalPos);
			const Vec3_ seedWorld = ShapeB->OldPos + QuatRotate(ShapeB->rotation, p.SeedPos);

			const CollisionInfoD3D info = ShapeA->PsDDADetection(seedWorld, dropPos);
			if (!info.Collision) continue;

			const Vec3_ rayDir = dropPos - seedWorld;
			const Vec3_ n = IncidentFaceNormal(info.Direction, rayDir);

			const Vec3_ normal = n; // 命中 A 的入射面 → 外向法线即 A→B
			const FLOAT_ separation = -std::abs(Dot(dropPos - info.pos, normal));
			const FLOAT_ friction = SQRT_(info.Friction * p.Friction);

			count = PushContact(contacts, count, info.pos, normal, separation, friction,
			                    FeatureId(p.Voxel, 1));
		}

		ReduceManifold(contacts, count);
		return count;
	}

	/**
	 * @brief 形状 vs 粒子（object1=Shape, object2=Particle）
	 * @details 粒子从 OldPos 运动到 pos，射入形状。命中面的外向法线指向
	 *          射线来向 = 形状→粒子，正好是所需的 normal(A→B)。
	 */
	unsigned int Collide(Contact3D *contacts, PhysicsShape3D *Shape, PhysicsParticle3D *Particle)
	{
		if (!Shape->DropCollision(Particle->pos).Collision)
		{
			// 粒子不在形状内，仍可能在本帧穿入，继续做射线检测
		}

		const CollisionInfoD3D info = Shape->PsDDADetection(Particle->OldPos, Particle->pos);
		if (!info.Collision)
		{
			return 0;
		}

		const Vec3_ rayDir = Particle->pos - Particle->OldPos;
		const Vec3_ normal = IncidentFaceNormal(info.Direction, rayDir); // Shape → Particle

		const FLOAT_ separation = -std::abs(Dot(Particle->pos - info.pos, normal));
		const FLOAT_ friction = SQRT_(info.Friction * Particle->friction);

		unsigned int count = PushContact(contacts, 0, info.pos, normal, separation, friction,
		                                 FeatureId(ToInt(info.pos), 0));

		// 防卡：把粒子的"旧位置"抬到接触面外侧，避免下一帧重复穿透
		Particle->OldPos = info.pos + normal * 0.1f;
		Particle->OldPosUpDataBool = false;

		return count;
	}

	/**
	 * @brief 形状 vs 地图（object1=Shape, object2=Map）
	 * @details 两轮：
	 *   1. 形状轮廓面 → 地形 DDA：命中地形的入射面，外向法线指向地形→形状，
	 *      故 normal(Shape→Map) = −n。
	 *   2. 地形最小轮廓点 → 形状反向射线：命中形状的入射面，外向法线指向
	 *      形状→地形轮廓点（即 Shape→Map），故 normal = +n。
	 */
	unsigned int Collide(Contact3D *contacts, PhysicsShape3D *Shape, MapFormwork3D *Map)
	{
		if (!Map) return 0;

		unsigned int count = 0;

		// ===== 第一轮：形状轮廓面 → 地形 =====
		for (size_t i = 0; i < Shape->OutlineSet.size() && count < PhysicsContactMaxSize3D; ++i)
		{
			const OutlinePoint3D &p = Shape->OutlineSet[i];

			const Vec3_ dropPos = Shape->pos + QuatRotate(Shape->rotation, p.LocalPos);
			const Vec3_ seedWorld = Shape->OldPos + QuatRotate(Shape->rotation, p.SeedPos);

			const CollisionInfoD3D info = Map->FMDDADetection(seedWorld, dropPos);
			if (!info.Collision) continue;

			const Vec3_ rayDir = dropPos - seedWorld;
			const Vec3_ n = IncidentFaceNormal(info.Direction, rayDir);

			const Vec3_ normal = -n; // 地形 → 形状 取反 = 形状 → 地形
			const FLOAT_ separation = -std::abs(Dot(dropPos - info.pos, normal));
			const FLOAT_ friction = SQRT_(info.Friction * p.Friction);

			count = PushContact(contacts, count, info.pos, normal, separation, friction,
			                    FeatureId(p.Voxel, 2));
		}

		// ===== 第二轮：地形最小轮廓 → 形状（反向射线）=====
		const Vec3_ mapCentrality = Map->FMGetCentrality();
		const int KD = 1;
		std::vector<MapOutline3D> outline;
		Map->FMGetMinOutline(
		    ToInt(Shape->pos.x - Shape->radius) - KD,
		    ToInt(Shape->pos.y - Shape->radius) - KD,
		    ToInt(Shape->pos.z - Shape->radius) - KD,
		    ToInt(Shape->pos.x + Shape->radius) + KD,
		    ToInt(Shape->pos.y + Shape->radius) + KD,
		    ToInt(Shape->pos.z + Shape->radius) + KD,
		    outline);

		for (size_t i = 0; i < outline.size() && count < PhysicsContactMaxSize3D; ++i)
		{
			const Vec3_ drop = outline[i].pos - mapCentrality; // 世界坐标

			// 快速过滤：不在形状内的地形轮廓点跳过
			if (!Shape->DropCollision(drop).Collision) continue;

			// 反向射线：从轮廓点关于形状质心的镜像点射向轮廓点
			const Vec3_ rayStart = drop - (Shape->pos - drop);
			const CollisionInfoD3D info = Shape->PsDDADetection(rayStart, drop);
			if (!info.Collision) continue;

			const Vec3_ rayDir = drop - rayStart;
			const Vec3_ normal = IncidentFaceNormal(info.Direction, rayDir); // 形状 → 地形

			const FLOAT_ separation = -std::abs(Dot(drop - info.pos, normal));
			const FLOAT_ friction = SQRT_(info.Friction * outline[i].F);

			count = PushContact(contacts, count, info.pos, normal, separation, friction,
			                    FeatureId(ToInt(drop), 3));
		}

		ReduceManifold(contacts, count);
		return count;
	}

	/**
	 * @brief 粒子 vs 地图（object1=Particle, object2=Map）
	 * @details 粒子射入地形，命中面外向法线指向地形→粒子，
	 *          故 normal(粒子→地形) = −n。
	 * @warning 2D 的既有缺陷：必须用浮点重载 `FMGetCollide(Vec2_)`，
	 *          它会先加 centrality；用 `ToInt(pos)` 走体素重载会漏掉偏移，
	 *          世界坐标为负时检测完全失效。3D 沿用浮点重载。
	 */
	unsigned int Collide(Contact3D *contacts, PhysicsParticle3D *Particle, MapFormwork3D *Map)
	{
		if (!Map) return 0;

		if (!Map->FMGetCollide(Particle->pos))
		{
			// 当前不在碰撞体内，仍继续做射线检测（可能本帧刚穿入）
		}

		const CollisionInfoD3D info = Map->FMDDADetection(Particle->OldPos, Particle->pos);
		if (!info.Collision)
		{
			return 0;
		}

		const Vec3_ rayDir = Particle->pos - Particle->OldPos;
		const Vec3_ normal = -IncidentFaceNormal(info.Direction, rayDir); // 粒子 → 地形

		const FLOAT_ separation = -std::abs(Dot(Particle->pos - info.pos, normal));
		const FLOAT_ friction = SQRT_(info.Friction * Particle->friction);

		unsigned int count = PushContact(contacts, 0, info.pos, normal, separation, friction,
		                                 FeatureId(ToInt(info.pos), 4));

		// 防卡（注意符号与 Shape-Particle 相反，沿用 2D 约定）
		Particle->OldPos = info.pos - normal * 0.1f;
		Particle->OldPosUpDataBool = false;

		return count;
	}

	/**
	 * @brief 圆球 vs 地图（object1=Circle, object2=Map）
	 * @details 第一轮：沿 6 个轴向的球面采样点射向地形，命中面外向法线
	 *          指向地形→球，取反得球→地形。
	 *          第二轮：地形最小轮廓点与球心求最近点，几何法线球→地形。
	 */
	unsigned int Collide(Contact3D *contacts, PhysicsCircle3D *Circle, MapFormwork3D *Map)
	{
		if (!Map) return 0;

		unsigned int count = 0;
		const Vec3_ mapCentrality = Map->FMGetCentrality();

		// ===== 第一轮：球面 6 向采样点 → 地形 DDA =====
		static const Vec3_ kDirs[6] = {
		    Vec3_(-1.0f, 0.0f, 0.0f), Vec3_(1.0f, 0.0f, 0.0f),
		    Vec3_(0.0f, -1.0f, 0.0f), Vec3_(0.0f, 1.0f, 0.0f),
		    Vec3_(0.0f, 0.0f, -1.0f), Vec3_(0.0f, 0.0f, 1.0f)};

		for (int i = 0; i < 6 && count < PhysicsContactMaxSize3D; ++i)
		{
			const Vec3_ sample = Circle->pos + kDirs[i] * Circle->radius;
			const CollisionInfoD3D info = Map->FMDDADetection(Circle->OldPos, sample);
			if (!info.Collision) continue;

			const Vec3_ rayDir = sample - Circle->OldPos;
			const Vec3_ normal = -IncidentFaceNormal(info.Direction, rayDir); // 球 → 地形
			const FLOAT_ separation = -std::abs(Dot(sample - info.pos, normal));
			const FLOAT_ friction = SQRT_(info.Friction * Circle->friction);

			count = PushContact(contacts, count, info.pos, normal, separation, friction,
			                    FeatureId(ToInt(sample), 5));
		}

		// ===== 第二轮：地形最小轮廓 → 球心最近点 =====
		const int KD = 2;
		std::vector<MapOutline3D> outline;
		Map->FMGetMinOutline(
		    ToInt(Circle->pos.x - Circle->radius) - KD,
		    ToInt(Circle->pos.y - Circle->radius) - KD,
		    ToInt(Circle->pos.z - Circle->radius) - KD,
		    ToInt(Circle->pos.x + Circle->radius) + KD,
		    ToInt(Circle->pos.y + Circle->radius) + KD,
		    ToInt(Circle->pos.z + Circle->radius) + KD,
		    outline);

		for (size_t i = 0; i < outline.size() && count < PhysicsContactMaxSize3D; ++i)
		{
			const Vec3_ d = outline[i].pos - mapCentrality - Circle->pos; // 球心 → 地形点
			const FLOAT_ len2 = ModulusLength(d);
			if (Circle->radius * Circle->radius <= len2) continue;

			const FLOAT_ len = SQRT_(len2);
			if (len < 1e-6f) continue;

			const Vec3_ normal = d / len; // 球心 → 地形点 = 球 → 地形
			const FLOAT_ separation = len - Circle->radius;
			const FLOAT_ friction = SQRT_(outline[i].F * Circle->friction);
			const Vec3_ position = normal * Circle->radius + Circle->pos;

			count = PushContact(contacts, count, position, normal, separation, friction,
			                    FeatureId(ToInt(outline[i].pos), 6));
		}

		ReduceManifold(contacts, count);
		return count;
	}

	/**
	 * @brief 圆球 vs 粒子（object1=Circle, object2=Particle）
	 * @details 纯几何：normal = normalize(粒子 − 球心) = 球→粒子。
	 */
	unsigned int Collide(Contact3D *contacts, PhysicsCircle3D *Circle, PhysicsParticle3D *Particle)
	{
		const Vec3_ dp = Particle->pos - Circle->pos;
		const FLOAT_ len2 = ModulusLength(dp);

		if (Circle->radius * Circle->radius <= len2)
		{
			return 0;
		}

		const FLOAT_ len = SQRT_(len2);
		if (len < 1e-6f)
		{
			return 0;
		}

		const Vec3_ normal = dp / len; // 球 → 粒子
		const FLOAT_ separation = len - Circle->radius;
		const FLOAT_ friction = SQRT_(Particle->friction * Circle->friction);
		const Vec3_ position = normal * Circle->radius + Circle->pos;

		return PushContact(contacts, 0, position, normal, separation, friction,
		                   FeatureId(ToInt(Particle->pos), 7));
	}

	/**
	 * @brief 圆球 vs 圆球（object1=CircleA, object2=CircleB）
	 * @details normal = normalize(B − A) = A→B。
	 */
	unsigned int Collide(Contact3D *contacts, PhysicsCircle3D *CircleA, PhysicsCircle3D *CircleB)
	{
		const Vec3_ dp = CircleB->pos - CircleA->pos;
		const FLOAT_ len2 = ModulusLength(dp);
		const FLOAT_ R = CircleA->radius + CircleB->radius;

		if (R * R <= len2)
		{
			return 0;
		}

		const FLOAT_ len = SQRT_(len2);
		if (len < 1e-6f)
		{
			return 0;
		}

		const Vec3_ normal = dp / len; // A → B
		const FLOAT_ separation = len - R;
		const FLOAT_ friction = SQRT_(CircleA->friction * CircleB->friction);
		const Vec3_ position = normal * CircleA->radius + CircleA->pos;

		return PushContact(contacts, 0, position, normal, separation, friction,
		                   FeatureId(ToInt(CircleA->pos), 8));
	}

	/**
	 * @brief 圆球 vs 形状（object1=Circle, object2=Shape）
	 * @details 两轮：
	 *   1. 球面 6 向采样点 → 形状 DDA：命中形状入射面，外向法线指向
	 *      形状→球（射线来向），取反得球→形状。
	 *   2. 形状轮廓面点 → 球心最近点：几何法线球→形状。
	 */
	unsigned int Collide(Contact3D *contacts, PhysicsCircle3D *Circle, PhysicsShape3D *Shape)
	{
		unsigned int count = 0;

		// ===== 第一轮：球面 6 向采样点 → 形状 =====
		static const Vec3_ kDirs[6] = {
		    Vec3_(-1.0f, 0.0f, 0.0f), Vec3_(1.0f, 0.0f, 0.0f),
		    Vec3_(0.0f, -1.0f, 0.0f), Vec3_(0.0f, 1.0f, 0.0f),
		    Vec3_(0.0f, 0.0f, -1.0f), Vec3_(0.0f, 0.0f, 1.0f)};

		for (int i = 0; i < 6 && count < PhysicsContactMaxSize3D; ++i)
		{
			const Vec3_ sample = Circle->pos + kDirs[i] * Circle->radius;
			const CollisionInfoD3D info = Shape->PsDDADetection(Circle->OldPos, sample);
			if (!info.Collision) continue;

			const Vec3_ rayDir = sample - Circle->OldPos;
			const Vec3_ n = IncidentFaceNormal(info.Direction, rayDir); // 形状 → 球
			const Vec3_ normal = -n;                                     // 球 → 形状
			const FLOAT_ separation = -std::abs(Dot(sample - info.pos, normal));
			const FLOAT_ friction = SQRT_(info.Friction * Circle->friction);

			count = PushContact(contacts, count, info.pos, normal, separation, friction,
			                    FeatureId(ToInt(sample), 9));
		}

		// ===== 第二轮：形状轮廓面点 → 球心 =====
		for (size_t i = 0; i < Shape->OutlineSet.size() && count < PhysicsContactMaxSize3D; ++i)
		{
			const OutlinePoint3D &p = Shape->OutlineSet[i];
			const Vec3_ surfaceWorld = Shape->pos + QuatRotate(Shape->rotation, p.LocalPos);
			const Vec3_ dp = surfaceWorld - Circle->pos; // 球心 → 形状面点

			const FLOAT_ len2 = ModulusLength(dp);
			if (Circle->radius * Circle->radius <= len2) continue;

			const FLOAT_ len = SQRT_(len2);
			if (len < 1e-6f) continue;

			const Vec3_ normal = dp / len; // 球 → 形状
			const FLOAT_ separation = len - Circle->radius;
			const FLOAT_ friction = SQRT_(p.Friction * Circle->friction);
			const Vec3_ position = Circle->pos + normal * Circle->radius;

			count = PushContact(contacts, count, position, normal, separation, friction,
			                    FeatureId(p.Voxel, 10));
		}

		ReduceManifold(contacts, count);
		return count;
	}

	/**
	 * @brief 线段 vs 形状（object1=Line, object2=Shape）
	 * @details 3D 中两异面线段一般不相交，故不能照搬 2D 的 segmentIntersection。
	 *          采用两轮：
	 *   1. 线段（胶囊轴）射入形状的 DDA：命中形状入射面，外向法线指向
	 *      射线来向（形状→线段），取反得线段→形状。
	 *   2. 形状轮廓面点与线段轴的最近点：若距离小于线段厚度则生成接触，
	 *      法线为线段轴→形状面点 = 线段→形状。
	 */
	unsigned int Collide(Contact3D *contacts, PhysicsLine3D *Line, PhysicsShape3D *Shape)
	{
		unsigned int count = 0;

		Vec3_ segBegin, segEnd;
		Line->GetSegment(segBegin, segEnd);

		// ===== 第一轮：线段轴 → 形状 =====
		{
			const CollisionInfoD3D info = Shape->PsDDADetection(segBegin, segEnd);
			if (info.Collision)
			{
				const Vec3_ rayDir = segEnd - segBegin;
				const Vec3_ n = IncidentFaceNormal(info.Direction, rayDir); // 形状 → 线段
				const Vec3_ normal = -n;                                     // 线段 → 形状
				const FLOAT_ separation = -std::abs(Dot(segBegin - info.pos, normal));
				const FLOAT_ friction = SQRT_(info.Friction * Line->friction);

				count = PushContact(contacts, count, info.pos, normal, separation, friction,
				                    FeatureId(ToInt(info.pos), 11));
			}
		}

		// ===== 第二轮：形状轮廓面点 → 线段轴最近点 =====
		for (size_t i = 0; i < Shape->OutlineSet.size() && count < PhysicsContactMaxSize3D; ++i)
		{
			const OutlinePoint3D &p = Shape->OutlineSet[i];
			const Vec3_ surfaceWorld = Shape->pos + QuatRotate(Shape->rotation, p.LocalPos);

			// 线段轴上的最近点（退化线段安全）
			const Vec3_ axis = segEnd - segBegin;
			const FLOAT_ axisLen2 = ModulusLength(axis);
			Vec3_ closest;
			if (axisLen2 < 1e-12f)
			{
				closest = segBegin;
			}
			else
			{
				const FLOAT_ t = Clamp(Dot(surfaceWorld - segBegin, axis) / axisLen2, 0.0f, 1.0f);
				closest = segBegin + axis * t;
			}

			const Vec3_ dp = surfaceWorld - closest; // 线段轴 → 形状面点
			const FLOAT_ dist2 = ModulusLength(dp);
			if (Line->Thickness * Line->Thickness <= dist2) continue;

			const FLOAT_ dist = SQRT_(dist2);
			if (dist < 1e-6f) continue;

			const Vec3_ normal = dp / dist; // 线段 → 形状
			const FLOAT_ separation = dist - Line->Thickness;
			const FLOAT_ friction = SQRT_(p.Friction * Line->friction);
			const Vec3_ position = closest + normal * Line->Thickness;

			count = PushContact(contacts, count, position, normal, separation, friction,
			                    FeatureId(p.Voxel, 12));
		}

		ReduceManifold(contacts, count);
		return count;
	}

	/**
	 * @brief 线段 vs 圆球（object1=Line, object2=Circle）
	 * @details 线段轴上取距球心最近点，若距离小于（球半径 + 线段厚度）则接触。
	 *          normal = normalize(球心 − 最近点) = 线段→球。
	 */
	unsigned int Collide(Contact3D *contacts, PhysicsLine3D *Line, PhysicsCircle3D *Circle)
	{
		Vec3_ segBegin, segEnd;
		Line->GetSegment(segBegin, segEnd);

		const Vec3_ axis = segEnd - segBegin;
		const FLOAT_ axisLen2 = ModulusLength(axis);

		Vec3_ closest;
		if (axisLen2 < 1e-12f)
		{
			closest = segBegin;
		}
		else
		{
			const FLOAT_ t = Clamp(Dot(Circle->pos - segBegin, axis) / axisLen2, 0.0f, 1.0f);
			closest = segBegin + axis * t;
		}

		const Vec3_ dp = Circle->pos - closest; // 线段 → 球心
		const FLOAT_ dist2 = ModulusLength(dp);
		const FLOAT_ R = Circle->radius + Line->Thickness;

		if (R * R <= dist2)
		{
			return 0;
		}

		const FLOAT_ dist = SQRT_(dist2);
		if (dist < 1e-6f)
		{
			return 0;
		}

		const Vec3_ normal = dp / dist; // 线段 → 球
		const FLOAT_ separation = dist - R;
		const FLOAT_ friction = SQRT_(Circle->friction * Line->friction);
		const Vec3_ position = closest + normal * Line->Thickness;

		return PushContact(contacts, 0, position, normal, separation, friction,
		                   FeatureId(ToInt(closest), 13));
	}

	/**
	 * @brief 线段 vs 粒子（object1=Line, object2=Particle）
	 * @details 3D 用「线段轴 vs 粒子本帧运动线段」的最近点对（异面直线）。
	 *          若最近距离小于线段厚度则接触，法线 = 线段轴→粒子。
	 */
	unsigned int Collide(Contact3D *contacts, PhysicsLine3D *Line, PhysicsParticle3D *Particle)
	{
		Vec3_ segBegin, segEnd;
		Line->GetSegment(segBegin, segEnd);

		Vec3_ c1, c2;
		FLOAT_ dist = 0.0f;
		ClosestSegmentSegment(segBegin, segEnd, Particle->OldPos, Particle->pos, c1, c2, dist);

		if (Line->Thickness <= dist)
		{
			return 0;
		}

		const Vec3_ dp = c2 - c1; // 线段轴 → 粒子轨迹
		const FLOAT_ len = Modulus(dp);
		if (len < 1e-6f)
		{
			return 0;
		}

		const Vec3_ normal = dp / len; // 线段 → 粒子
		const FLOAT_ separation = dist - Line->Thickness;
		const FLOAT_ friction = SQRT_(Particle->friction * Line->friction);
		const Vec3_ position = c1 + normal * Line->Thickness;

		unsigned int count = PushContact(contacts, 0, position, normal, separation, friction,
		                                 FeatureId(ToInt(c1), 14));

		// 防卡：与 2D Line-Particle 一致，把粒子旧位置抬出接触面
		Particle->OldPos = position - normal * 0.1f;
		Particle->OldPosUpDataBool = false;

		return count;
	}

	/**
	 * @brief 线段 vs 地图（object1=Line, object2=Map）
	 * @details 两轮：
	 *   1. 线段轴射入地形 DDA：命中地形入射面，外向法线指向地形→线段，
	 *      取反得线段→地形。
	 *   2. 地形最小轮廓点与线段轴最近点：法线为线段轴→地形轮廓点。
	 */
	unsigned int Collide(Contact3D *contacts, PhysicsLine3D *Line, MapFormwork3D *Map)
	{
		if (!Map) return 0;

		unsigned int count = 0;
		const Vec3_ mapCentrality = Map->FMGetCentrality();

		Vec3_ segBegin, segEnd;
		Line->GetSegment(segBegin, segEnd);

		// ===== 第一轮：线段轴 → 地形 =====
		{
			const CollisionInfoD3D info = Map->FMDDADetection(segBegin, segEnd);
			if (info.Collision)
			{
				const Vec3_ rayDir = segEnd - segBegin;
				const Vec3_ n = IncidentFaceNormal(info.Direction, rayDir); // 地形 → 线段
				const Vec3_ normal = -n;                                     // 线段 → 地形
				const FLOAT_ separation = -std::abs(Dot(segBegin - info.pos, normal));
				const FLOAT_ friction = SQRT_(info.Friction * Line->friction);

				count = PushContact(contacts, count, info.pos, normal, separation, friction,
				                    FeatureId(ToInt(info.pos), 15));
			}
		}

		// ===== 第二轮：地形最小轮廓 → 线段轴最近点 =====
		const int KD = 2;
		std::vector<MapOutline3D> outline;
		Map->FMGetMinOutline(
		    ToInt(Line->pos.x - Line->radius) - KD,
		    ToInt(Line->pos.y - Line->radius) - KD,
		    ToInt(Line->pos.z - Line->radius) - KD,
		    ToInt(Line->pos.x + Line->radius) + KD,
		    ToInt(Line->pos.y + Line->radius) + KD,
		    ToInt(Line->pos.z + Line->radius) + KD,
		    outline);

		const Vec3_ axis = segEnd - segBegin;
		const FLOAT_ axisLen2 = ModulusLength(axis);

		for (size_t i = 0; i < outline.size() && count < PhysicsContactMaxSize3D; ++i)
		{
			const Vec3_ mapPoint = outline[i].pos - mapCentrality; // 世界坐标

			Vec3_ closest;
			if (axisLen2 < 1e-12f)
			{
				closest = segBegin;
			}
			else
			{
				const FLOAT_ t = Clamp(Dot(mapPoint - segBegin, axis) / axisLen2, 0.0f, 1.0f);
				closest = segBegin + axis * t;
			}

			const Vec3_ dp = mapPoint - closest; // 线段轴 → 地形轮廓点
			const FLOAT_ dist2 = ModulusLength(dp);
			if (Line->Thickness * Line->Thickness <= dist2) continue;

			const FLOAT_ dist = SQRT_(dist2);
			if (dist < 1e-6f) continue;

			const Vec3_ normal = dp / dist; // 线段 → 地形
			const FLOAT_ separation = dist - Line->Thickness;
			const FLOAT_ friction = SQRT_(outline[i].F * Line->friction);
			const Vec3_ position = closest + normal * Line->Thickness;

			count = PushContact(contacts, count, position, normal, separation, friction,
			                    FeatureId(ToInt(mapPoint), 16));
		}

		ReduceManifold(contacts, count);
		return count;
	}

}
