#include "BaseCalculate3D.hpp"
#include <algorithm>

// ═══════════════════════════════════════════════════════════════════════════
// PhysicsBlock3D 数学与几何工具实现
// 由 PixelClean/PhysicsBlock/BaseCalculate.cpp 逐函数改写为 3D。
// ═══════════════════════════════════════════════════════════════════════════

namespace PhysicsBlock3D
{

	// ───────────────────────── 快速平方根 ─────────────────────────

	/**
	 * @brief 快速平方根（Quake III 位技巧 + 牛顿迭代）
	 * @note 与 2D 版实现相同；对 3D 而言无维度差异。 */
	float q_sqrt(float S)
	{
		if (S <= 0.0f)
		{
			return 0.0f;
		}
		long i;
		float x2, y;
		const float threehalfs = 1.5F;

		x2 = S * 0.5F;
		y = S;
		i = *(long *)&y;          // 位级重解释
		i = 0x5f3759df - (i >> 1); // 初始猜测
		y = *(float *)&i;
		y = y * (threehalfs - (x2 * y * y)); // 一次牛顿迭代
		return y * S;                        // 1/sqrt(S) * S = sqrt(S)
	}

	double q_sqrt(double S)
	{
		if (S <= 0.0)
		{
			return 0.0;
		}
		return std::sqrt(S);
	}

	// ───────────────────────── 线段-盒 裁剪（slab 方法） ─────────────────────────

	BoxFocus LineBoxFocus(Vec3_ start, Vec3_ end, const Vec3_ size)
	{
		BoxFocus result;
		result.Focus = false;
		result.start = start;
		result.end = end;

		const Vec3_ dir = end - start;
		const Vec3_ boxMin(0.0f, 0.0f, 0.0f);
		const Vec3_ boxMax = size;

		FLOAT_ t0 = 0.0f; // 参数区间下界
		FLOAT_ t1 = 1.0f; // 参数区间上界

		// 逐轴做 slab 裁剪，取区间交集
		for (int axis = 0; axis < 3; ++axis)
		{
			const FLOAT_ d = dir[axis];
			const FLOAT_ s = start[axis];
			const FLOAT_ lo = boxMin[axis];
			const FLOAT_ hi = boxMax[axis];

			if (std::abs(d) < FLOAT_(1e-12))
			{
				// 该轴上方向分量为零：起点必须已在 slab 内，否则整段在盒外
				if (s < lo || s > hi)
				{
					return result; // 不相交
				}
				continue;
			}

			FLOAT_ tNear = (lo - s) / d;
			FLOAT_ tFar = (hi - s) / d;
			if (tNear > tFar)
			{
				std::swap(tNear, tFar);
			}
			if (tNear > t0)
			{
				t0 = tNear;
			}
			if (tFar < t1)
			{
				t1 = tFar;
			}
			if (t0 > t1)
			{
				return result; // 区间为空，不相交
			}
		}

		result.Focus = true;
		result.start = start + dir * t0;
		result.end = start + dir * t1;
		return result;
	}

	// ───────────────────────── 盒-盒 推出（AABB） ─────────────────────────

	Vec3_ BoxToBox(Vec3_ centerA, Vec3_ halfA, Vec3_ centerB, Vec3_ halfB)
	{
		// 各轴重叠量；任一轴不重叠则无碰撞
		const FLOAT_ overlapX = (halfA.x + halfB.x) - std::abs(centerB.x - centerA.x);
		const FLOAT_ overlapY = (halfA.y + halfB.y) - std::abs(centerB.y - centerA.y);
		const FLOAT_ overlapZ = (halfA.z + halfB.z) - std::abs(centerB.z - centerA.z);

		if (overlapX <= 0.0f || overlapY <= 0.0f || overlapZ <= 0.0f)
		{
			return Vec3_(0.0f, 0.0f, 0.0f);
		}

		// 取重叠最小的轴作为分离轴（MTV）
		if (overlapX <= overlapY && overlapX <= overlapZ)
		{
			const FLOAT_ sign = (centerB.x >= centerA.x) ? 1.0f : -1.0f;
			return Vec3_(overlapX * sign, 0.0f, 0.0f);
		}
		if (overlapY <= overlapZ)
		{
			const FLOAT_ sign = (centerB.y >= centerA.y) ? 1.0f : -1.0f;
			return Vec3_(0.0f, overlapY * sign, 0.0f);
		}
		const FLOAT_ sign = (centerB.z >= centerA.z) ? 1.0f : -1.0f;
		return Vec3_(0.0f, 0.0f, overlapZ * sign);
	}

	// ───────────────────────── 盒-点 推出 ─────────────────────────

	Vec3_ BoxToDrop(FLOAT_ A1, FLOAT_ A2, FLOAT_ B1, FLOAT_ B2,
	                FLOAT_ C1, FLOAT_ C2, Vec3_ Drop, Vec3_ PY)
	{
		// 点必须在盒内才需要推出
		if (Drop.x < A1 || Drop.x > A2 ||
		    Drop.y < B1 || Drop.y > B2 ||
		    Drop.z < C1 || Drop.z > C2)
		{
			return Vec3_(0.0f, 0.0f, 0.0f);
		}

		// 各轴到最近面的距离（正数）
		const FLOAT_ dX = std::min(Drop.x - A1, A2 - Drop.x);
		const FLOAT_ dY = std::min(Drop.y - B1, B2 - Drop.y);
		const FLOAT_ dZ = std::min(Drop.z - C1, C2 - Drop.z);

		// 2D 版语义：按期望方向 PY 的分量决定优先推出轴。
		// 3D 扩展：只考虑 PY 分量非零的轴，其中取穿透最浅者；
		// 若 PY 全为零（无偏好），则三轴中取穿透最浅者。
		int axis = -1;
		FLOAT_ best = FLOAT_MAX;

		const FLOAT_ comp[3] = {PY.x, PY.y, PY.z};
		const FLOAT_ depth[3] = {dX, dY, dZ};

		for (int i = 0; i < 3; ++i)
		{
			if (comp[i] != 0.0f && depth[i] < best)
			{
				best = depth[i];
				axis = i;
			}
		}
		if (axis < 0)
		{
			// 无方向偏好：选穿透最浅的轴
			for (int i = 0; i < 3; ++i)
			{
				if (depth[i] < best)
				{
					best = depth[i];
					axis = i;
				}
			}
		}

		Vec3_ result(0.0f, 0.0f, 0.0f);
		if (axis < 0)
		{
			return result;
		}

		// 推出方向：优先与 PY 分量符号一致；PY 分量为零则推向最近的面
		const FLOAT_ lo[3] = {A1, B1, C1};
		const FLOAT_ hi[3] = {A2, B2, C2};
		FLOAT_ sign;
		if (comp[axis] > 0.0f)
		{
			sign = 1.0f;
		}
		else if (comp[axis] < 0.0f)
		{
			sign = -1.0f;
		}
		else
		{
			// 推向较近的一面
			sign = ((Drop[axis] - lo[axis]) < (hi[axis] - Drop[axis])) ? -1.0f : 1.0f;
		}

		// 推出量 = 该轴到对应面的距离（略加冗余避免残留重叠）
		const FLOAT_ push = (sign > 0.0f) ? (hi[axis] - Drop[axis]) : (lo[axis] - Drop[axis]);
		result[axis] = push;
		return result;
	}

	Vec3_ BoxToDrop(FLOAT_ R, Vec3_ Drop, Vec3_ PY)
	{
		return BoxToDrop(-R, R, -R, R, -R, R, Drop, PY);
	}

	// ───────────────────────── 力矩 / 力分解 ─────────────────────────

	Vec3_ TorqueCalculate(Vec3_ Barycenter, Vec3_ Spot, Vec3_ Force)
	{
		// τ = r × F，r 为质心指向受力点的力臂
		return Cross(Spot - Barycenter, Force);
	}

	FLOAT_ CalculateIncludedAngle(Vec3_ V1, Vec3_ V2)
	{
		// atan2(|a×b|, a·b)：比 acos(a·b/(|a||b|)) 在近 0/π 时更稳定
		const FLOAT_ crossLen = Modulus(Cross(V1, V2));
		const FLOAT_ dotVal = Dot(V1, V2);
		if (crossLen == 0.0f && dotVal == 0.0f)
		{
			return 0.0f;
		}
		return atan2f(crossLen, dotVal);
	}

	DecompositionForce3D CalculateDecompositionForce(Vec3_ angle, Vec3_ force)
	{
		DecompositionForce3D result;
		// 参考法向：angle 归一化（零向量回退为 +Y，与 SafeNormalize 一致）
		const Vec3_ normal = SafeNormalize(angle);
		// 直接投影：垂直分量 = n̂·(F·n̂)，平行分量 = F - 垂直分量
		const FLOAT_ proj = Dot(force, normal);
		result.Normal = normal;
		result.Vartical = normal * proj;
		result.Parallel = force - result.Vartical;
		return result;
	}

	DecompositionForceVal3D CalculateDecompositionForceVal(Vec3_ angle, Vec3_ force)
	{
		DecompositionForceVal3D result;
		const Vec3_ normal = SafeNormalize(angle);
		const FLOAT_ proj = Dot(force, normal);
		result.Vartical = std::abs(proj);
		// 平行分量大小 = sqrt(|F|² - 垂直分量²)，夹到非负避免浮点误差
		const FLOAT_ f2 = ModulusLength(force);
		const FLOAT_ p2 = proj * proj;
		result.Parallel = (f2 > p2) ? SQRT_(f2 - p2) : 0.0f;
		return result;
	}

	// ───────────────────────── Morton 3D（64 位） ─────────────────────────

	/**
	 * @brief 把 21 bit 的整数按 3 位间隔展开
	 * @note 经典 libmorton 的 splitBy3 掩码序列。 */
	static inline uint_fast64_t SplitBy3(uint_fast32_t a)
	{
		uint_fast64_t x = a & 0x1fffffULL; // 只取低 21 bit
		x = (x | x << 32) & 0x1f00000000ffffULL;
		x = (x | x << 16) & 0x1f0000ff0000ffULL;
		x = (x | x << 8) & 0x100f00f00f00f00fULL;
		x = (x | x << 4) & 0x10c30c30c30c30c3ULL;
		x = (x | x << 2) & 0x1249249249249249ULL;
		return x;
	}

	/**
	 * @brief SplitBy3 的逆操作：把 3 位间隔的位收拢回连续 21 bit */
	static inline uint_fast32_t CompactBy3(uint_fast64_t x)
	{
		x &= 0x1249249249249249ULL;
		x = (x | x >> 2) & 0x10c30c30c30c30c3ULL;
		x = (x | x >> 4) & 0x100f00f00f00f00fULL;
		x = (x | x >> 8) & 0x1f0000ff0000ffULL;
		x = (x | x >> 16) & 0x1f00000000ffffULL;
		x = (x | x >> 32) & 0x1fffffULL;
		return (uint_fast32_t)x;
	}

	uint_fast64_t Morton3D(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z)
	{
		return SplitBy3(x) | (SplitBy3(y) << 1) | (SplitBy3(z) << 2);
	}

	void Morton3DInverse(uint_fast64_t code, uint_fast32_t &x, uint_fast32_t &y, uint_fast32_t &z)
	{
		x = CompactBy3(code);
		y = CompactBy3(code >> 1);
		z = CompactBy3(code >> 2);
	}

	// ───────────────────────── 点-线距离 ─────────────────────────

	FLOAT_ DropUptoLineShortes(Vec3_ start, Vec3_ end, Vec3_ drop)
	{
		const Vec3_ ab = end - start;
		const FLOAT_ len2 = ModulusLength(ab);
		if (len2 <= FLOAT_(1e-12))
		{
			return Modulus(drop - start); // 退化为点
		}
		// 注意：2D 版误用平方长度做分母，此处修正为除以平方长度
		FLOAT_ t = Dot(drop - start, ab) / len2;
		t = Clamp(t, 0.0f, 1.0f);
		return Modulus(drop - (start + ab * t));
	}

	Vec3_ DropLineShortesIntersect(Vec3_ start, Vec3_ end, Vec3_ drop)
	{
		const Vec3_ ab = end - start;
		const FLOAT_ len2 = ModulusLength(ab);
		if (len2 <= FLOAT_(1e-12))
		{
			return start;
		}
		// 不夹 t：交点可落在延长线上
		const FLOAT_ t = Dot(drop - start, ab) / len2;
		return start + ab * t;
	}

	Vec3_ DropUptoLineShortesIntersect(Vec3_ start, Vec3_ end, Vec3_ drop)
	{
		const Vec3_ ab = end - start;
		const FLOAT_ len2 = ModulusLength(ab);
		if (len2 <= FLOAT_(1e-12))
		{
			return start;
		}
		FLOAT_ t = Dot(drop - start, ab) / len2;
		t = Clamp(t, 0.0f, 1.0f);
		return start + ab * t;
	}

	// ───────────────────────── 通用小工具 ─────────────────────────

	FLOAT_ Clamp(FLOAT_ a, FLOAT_ low, FLOAT_ high)
	{
		return std::max(low, std::min(a, high));
	}

	// ═══════════════════════════════════════════════════════════════════════
	// 3D 专有几何
	// ═══════════════════════════════════════════════════════════════════════

	bool AABBIntersect(const Vec3_ &centerA, const Vec3_ &halfA,
	                   const Vec3_ &centerB, const Vec3_ &halfB)
	{
		return std::abs(centerB.x - centerA.x) <= (halfA.x + halfB.x) &&
		       std::abs(centerB.y - centerA.y) <= (halfA.y + halfB.y) &&
		       std::abs(centerB.z - centerA.z) <= (halfA.z + halfB.z);
	}

	bool RayAABB(const Vec3_ &origin, const Vec3_ &dir,
	             const Vec3_ &boxMin, const Vec3_ &boxMax,
	             FLOAT_ &tMin, FLOAT_ &tMax)
	{
		FLOAT_ t0 = -FLOAT_MAX;
		FLOAT_ t1 = FLOAT_MAX;

		for (int axis = 0; axis < 3; ++axis)
		{
			const FLOAT_ d = dir[axis];
			const FLOAT_ o = origin[axis];

			if (std::abs(d) < FLOAT_(1e-12))
			{
				// 平行于该轴：起点必须在该 slab 内
				if (o < boxMin[axis] || o > boxMax[axis])
				{
					return false;
				}
				continue;
			}

			FLOAT_ tn = (boxMin[axis] - o) / d;
			FLOAT_ tf = (boxMax[axis] - o) / d;
			if (tn > tf)
			{
				std::swap(tn, tf);
			}
			if (tn > t0)
			{
				t0 = tn;
			}
			if (tf < t1)
			{
				t1 = tf;
			}
			if (t0 > t1)
			{
				return false;
			}
		}

		tMin = t0;
		tMax = t1;
		return true;
	}

	/**
	 * @brief 把 OBB 投影到轴 n 上的半径
	 * @details r = Σ |n·axis_i| · half_i，其中 axis_i 是旋转矩阵的列向量 */
	static inline FLOAT_ OBBProjectionRadius(const Vec3_ &n, const Vec3_ &half, const Mat3_ &rot)
	{
		return std::abs(Dot(n, rot[0])) * half.x +
		       std::abs(Dot(n, rot[1])) * half.y +
		       std::abs(Dot(n, rot[2])) * half.z;
	}

	bool SATTest(const Vec3_ *axes, int axisCount,
	             const Vec3_ &centerA, const Vec3_ &halfA, const Mat3_ &rotA,
	             const Vec3_ &centerB, const Vec3_ &halfB, const Mat3_ &rotB,
	             Vec3_ &outNormal, FLOAT_ &outDepth)
	{
		const Vec3_ delta = centerB - centerA;
		FLOAT_ minOverlap = FLOAT_MAX;
		Vec3_ bestAxis(0.0f, 1.0f, 0.0f);
		bool found = false;

		for (int i = 0; i < axisCount; ++i)
		{
			const Vec3_ n = axes[i];
			const FLOAT_ len2 = ModulusLength(n);
			if (len2 < FLOAT_(1e-12))
			{
				continue; // 退化轴（平行叉积）跳过
			}
			const Vec3_ axis = n / SQRT_(len2);

			const FLOAT_ dist = std::abs(Dot(axis, delta));
			const FLOAT_ ra = OBBProjectionRadius(axis, halfA, rotA);
			const FLOAT_ rb = OBBProjectionRadius(axis, halfB, rotB);
			const FLOAT_ overlap = ra + rb - dist;

			if (overlap <= 0.0f)
			{
				// 找到分离轴 → 无碰撞
				outNormal = axis;
				outDepth = 0.0f;
				return true;
			}

			if (overlap < minOverlap)
			{
				minOverlap = overlap;
				bestAxis = axis;
				found = true;
			}
		}

		if (!found)
		{
			// 所有轴都退化：保守判定为分离，避免产生 NaN 法线
			outNormal = Vec3_(0.0f, 1.0f, 0.0f);
			outDepth = 0.0f;
			return true;
		}

		// 法线统一指向 A → B 方向
		if (Dot(bestAxis, delta) < 0.0f)
		{
			bestAxis = -bestAxis;
		}
		outNormal = bestAxis;
		outDepth = minOverlap;
		return false;
	}

	void BuildSATAxes(const Mat3_ &rotA, const Mat3_ &rotB, Vec3_ *out, int &outCount)
	{
		int n = 0;
		// 3 条 A 的轴
		out[n++] = rotA[0];
		out[n++] = rotA[1];
		out[n++] = rotA[2];
		// 3 条 B 的轴
		out[n++] = rotB[0];
		out[n++] = rotB[1];
		out[n++] = rotB[2];
		// 9 条叉积轴（平行时会退化，SATTest 内部会跳过零长轴）
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				out[n++] = Cross(rotA[i], rotB[j]);
			}
		}
		outCount = n;
	}

	void ClipPolygonToPlane(const Vec3_ *polyIn, int countIn,
	                        const Vec3_ &planeNormal, FLOAT_ planeD,
	                        Vec3_ *polyOut, int &countOut)
	{
		countOut = 0;
		if (countIn <= 0)
		{
			return;
		}

		for (int i = 0; i < countIn; ++i)
		{
			const Vec3_ &cur = polyIn[i];
			const Vec3_ &nxt = polyIn[(i + 1) % countIn];

			const FLOAT_ dCur = Dot(planeNormal, cur) - planeD;
			const FLOAT_ dNxt = Dot(planeNormal, nxt) - planeD;

			const bool curIn = (dCur <= 0.0f);
			const bool nxtIn = (dNxt <= 0.0f);

			if (curIn)
			{
				polyOut[countOut++] = cur;
			}

			// 跨越平面：插入交点
			if (curIn != nxtIn)
			{
				const FLOAT_ denom = dCur - dNxt;
				if (std::abs(denom) > FLOAT_(1e-12))
				{
					const FLOAT_ t = dCur / denom;
					polyOut[countOut++] = cur + (nxt - cur) * t;
				}
			}
		}
	}

	FLOAT_ PolygonArea(const Vec3_ *poly, int count, const Vec3_ &normal)
	{
		if (count < 3)
		{
			return 0.0f;
		}
		// 三角形扇形求和：A = 0.5 |Σ (p_i - p_0) × (p_{i+1} - p_0)|
		Vec3_ acc(0.0f, 0.0f, 0.0f);
		const Vec3_ &p0 = poly[0];
		for (int i = 1; i + 1 < count; ++i)
		{
			acc += Cross(poly[i] - p0, poly[i + 1] - p0);
		}
		// 投影到法向取绝对值（消除绕向符号）
		return FLOAT_(0.5) * std::abs(Dot(acc, normal));
	}

	void BuildTangentBasis(const Vec3_ &normal, Vec3_ &tangent1, Vec3_ &tangent2)
	{
		const Vec3_ n = SafeNormalize(normal);
		// 选一个与 n 不平行的辅助轴
		const Vec3_ helper = (std::abs(n.x) < FLOAT_(0.9)) ? Vec3_(1.0f, 0.0f, 0.0f)
		                                                   : Vec3_(0.0f, 1.0f, 0.0f);
		tangent1 = SafeNormalize(Cross(n, helper), Vec3_(1.0f, 0.0f, 0.0f));
		tangent2 = Cross(n, tangent1); // 已单位化：n 与 t1 正交且均为单位向量
	}

	Vec3_ ProjectFrictionDisk(const Vec3_ &tangentImpulse, FLOAT_ maxFriction)
	{
		const FLOAT_ len2 = ModulusLength(tangentImpulse);
		if (len2 <= maxFriction * maxFriction)
		{
			return tangentImpulse; // 在摩擦圆盘内，直接接受
		}
		const FLOAT_ len = SQRT_(len2);
		if (len <= FLOAT_(1e-12))
		{
			return Vec3_(0.0f, 0.0f, 0.0f);
		}
		// 投影到圆盘边界
		return tangentImpulse * (maxFriction / len);
	}

	Mat3_ SymmetricInverse(const Mat3_ &m)
	{
		// 余子式 / 行列式；奇异则返回零矩阵
		const FLOAT_ a = m[0][0], b = m[0][1], c = m[0][2];
		const FLOAT_ d = m[1][0], e = m[1][1], f = m[1][2];
		const FLOAT_ g = m[2][0], h = m[2][1], i = m[2][2];

		const FLOAT_ A = (e * i - f * h);
		const FLOAT_ B = -(d * i - f * g);
		const FLOAT_ C = (d * h - e * g);

		const FLOAT_ det = a * A + b * B + c * C;
		if (std::abs(det) < FLOAT_(1e-12))
		{
			return Mat3_(0.0f);
		}

		const FLOAT_ invDet = FLOAT_(1.0) / det;

		Mat3_ result;
		// 伴随矩阵（列主序：result[col][row]）
		result[0][0] = A * invDet;
		result[1][0] = B * invDet;
		result[2][0] = C * invDet;
		result[0][1] = (-(b * i - c * h)) * invDet;
		result[1][1] = (a * i - c * g) * invDet;
		result[2][1] = (-(a * h - b * g)) * invDet;
		result[0][2] = (b * f - c * e) * invDet;
		result[1][2] = (-(a * f - c * d)) * invDet;
		result[2][2] = (a * e - b * d) * invDet;
		return result;
	}

}
