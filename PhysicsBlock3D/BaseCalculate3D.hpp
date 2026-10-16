#pragma once
#include "BaseStruct3D.hpp"
#include <cmath>
#include <cstdint>

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

// ═══════════════════════════════════════════════════════════════════════════
// PhysicsBlock3D 数学与几何工具
// 由 PixelClean/PhysicsBlock/BaseCalculate.{hpp,cpp} 逐函数改写为 3D。
//
// 关键设计变化（相对 2D）：
//  1. 角度分解链 CalculateDecompositionForce(Vec2_,Vec2_) 内部用 acos/cos/sin
//     做"角度链式分解"，3D 下改为直接向量投影（点乘），无三角函数、
//     无除零风险、结果精确。语义等价但更稳。
//  2. AngleMat{Cos,Sin} → 四元数 Quat_（见 BaseStruct3D.hpp 的 QuatRotate）。
//  3. Morton2D → Morton3D（64 位，位交错 3 路，支持每轴 ≤21 bit）。
//  4. Bresenham → Amanatides-Woo 3D DDA（整数体素遍历，无 tie-break 歧义）。
//  5. 新增 3D 专有几何：AABB 相交/最近点、SAT 轴测试、射线-AABB、
//     面裁剪生成接触流形。
// ═══════════════════════════════════════════════════════════════════════════

namespace PhysicsBlock3D
{

	// ───────────────────────── 标量/坐标转换 ─────────────────────────

	/**
	 * @brief double 转 int，向下取整（与 2D 版语义一致） */
	int inline ToInt(double val)
	{
		return val >= 0 ? static_cast<int>(val) : (static_cast<int>(val) - 1);
	}

	/**
	 * @brief float 转 int，向下取整 */
	int inline ToInt(float val)
	{
		return val >= 0 ? static_cast<int>(val) : (static_cast<int>(val) - 1);
	}

	/**
	 * @brief vec3 转 ivec3，向下取整 */
	IVec3_ inline ToInt(Vec3_ val)
	{
		return {ToInt(val.x), ToInt(val.y), ToInt(val.z)};
	}

	/**
	 * @brief dvec3 转 ivec3，向下取整 */
	IVec3_ inline ToInt(DVec3_ val)
	{
		return {ToInt(val.x), ToInt(val.y), ToInt(val.z)};
	}

	/**
	 * @brief 快速平方根（牛顿迭代法近似）
	 * @warning 现代 CPU 有专用开方指令，此函数可能比标准 sqrt() 慢 */
	float q_sqrt(float S);
	double q_sqrt(double S);

	// ───────────────────────── 向量模长 ─────────────────────────

	/**
	 * @brief 获取向量的模的平方（x²+y²+z²），避免开方 */
	FLOAT_ inline ModulusLength(const Vec3_ &Modulus)
	{
		return ((Modulus.x * Modulus.x) + (Modulus.y * Modulus.y) + (Modulus.z * Modulus.z));
	}

	/**
	 * @brief 获取向量的模的长度 sqrt(x²+y²+z²) */
	FLOAT_ inline Modulus(const Vec3_ &Modulus)
	{
		return SQRT_((Modulus.x * Modulus.x) + (Modulus.y * Modulus.y) + (Modulus.z * Modulus.z));
	}

	/**
	 * @brief 安全归一化（零向量返回 fallback，避免 NaN）
	 * @param v 输入向量
	 * @param fallback 模长过小时的返回值
	 * @param eps 视为零的阈值 */
	Vec3_ inline SafeNormalize(const Vec3_ &v, const Vec3_ &fallback = Vec3_(0.0f, 1.0f, 0.0f),
	                           FLOAT_ eps = FLOAT_(1e-8))
	{
		const FLOAT_ len2 = ModulusLength(v);
		if (len2 <= eps * eps)
		{
			return fallback;
		}
		return v / SQRT_(len2);
	}

	// ───────────────────────── 点积 / 叉积（3D） ─────────────────────────

	/**
	 * @brief 向量点积 a·b */
	FLOAT_ inline Dot(const Vec3_ &a, const Vec3_ &b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	/**
	 * @brief 向量叉积 a×b（3D 结果仍是向量）
	 * @note 2D 的 Cross 返回标量（z 分量）；3D 直接返回完整向量。 */
	Vec3_ inline Cross(const Vec3_ &a, const Vec3_ &b)
	{
		return Vec3_(
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x);
	}

	/**
	 * @brief 标量 × 向量 的叉积形式 Cross(s, a) = s·a
	 * @note 2D 版 Cross(FLOAT_, Vec2_) 是"逆时针旋转 90° 并缩放"；
	 *       3D 下"标量与向量叉乘"无几何意义，故此处退化为缩放，
	 *       保留函数名只为 2D 代码平移方便。 */
	Vec3_ inline Cross(FLOAT_ s, const Vec3_ &a)
	{
		return Vec3_(s * a.x, s * a.y, s * a.z);
	}

	/**
	 * @brief Cross(a, s) = a·s（同上，退化形式） */
	Vec3_ inline Cross(const Vec3_ &a, FLOAT_ s)
	{
		return Vec3_(s * a.x, s * a.y, s * a.z);
	}

	// ───────────────────────── 旋转（四元数） ─────────────────────────

	/**
	 * @brief 用四元数旋转向量（2D 的 vec2angle(pos, angle) 对应物） */
	Vec3_ inline vec2angle(Vec3_ pos, const Quat_ &q)
	{
		return q * pos;
	}

	/**
	 * @brief 绕某个点旋转（2D vec2PosAngle 对应物） */
	Vec3_ inline vec2PosAngle(Vec3_ pos, Vec3_ lingdian, const Quat_ &q)
	{
		return (q * (pos - lingdian)) + lingdian;
	}

	/**
	 * @brief 坐标点转换坐标系后取整
	 * @param Pos 需要转换的点
	 * @param xPos 坐标系原点偏移
	 * @param q 坐标系旋转（四元数）
	 * @details 2D 版是 ToInt(vec2angle(xPos - Pos, angle))；3D 用逆旋转把
	 *          Pos 变到以 xPos 为原点的局部系。 */
	IVec3_ inline ToIntPos(Vec3_ Pos, Vec3_ xPos, const Quat_ &q)
	{
		return ToInt(glm::inverse(q) * (Pos - xPos));
	}

	// ───────────────────────── 角度辅助（保留给标量场景） ─────────────────────────

	/**
	 * @brief 根据角度计算 cos/sin（2D 遗留辅助，3D 主路径用四元数） */
	Vec3_ inline AngleFloatToAngleVec(FLOAT_ angle)
	{
		return {cosf(angle), sinf(angle), 0.0f};
	}

	/**
	 * @brief 向量在 XZ 平面内与 +X 轴的夹角（绕 Y 轴）
	 * @note 2D EdgeVecToCosAngleFloat 的 3D 简化对应物；仅在需要标量
	 *       偏航角的场合使用（如调试输出），主仿真路径不使用。 */
	FLOAT_ inline EdgeVecToCosAngleFloat(Vec3_ XYedge)
	{
		if (XYedge.x == 0.0f && XYedge.z == 0.0f)
		{
			return 0.0f;
		}
		return atan2f(XYedge.z, XYedge.x);
	}

	// ───────────────────────── 线段裁剪 ─────────────────────────

	/**
	 * @brief 裁剪出被轴对齐盒覆盖的线段（2D LineSquareFocus 的 3D 版）
	 * @param start 起始位置
	 * @param end 结束位置
	 * @param size 盒尺寸（盒为 [0,size.x]×[0,size.y]×[0,size.z]）
	 * @return BoxFocus，包含是否相交和裁剪后的起止位置
	 * @details 用 slab 方法（逐轴求 t 区间并取交），替代 2D 的手写三分支裁剪。
	 *          2D 版对垂直线段/水平线段单独处理，3D 的 slab 方法天然统一。 */
	BoxFocus LineBoxFocus(Vec3_ start, Vec3_ end, const Vec3_ size);

	// ───────────────────────── 盒-盒 / 盒-点 碰撞 ─────────────────────────

	/**
	 * @brief 轴对齐盒与轴对齐盒的推出量（A 静态，B 动态）
	 * @return 返回 B 应该偏移的最小平移向量（MTV），无重叠返回零向量
	 * @details 2D SquareToSquare 用两坐标系各测 4 角；3D AABB 直接比较
	 *          各轴重叠量取最小轴即可，精确且 O(1)。 */
	Vec3_ BoxToBox(Vec3_ centerA, Vec3_ halfA, Vec3_ centerB, Vec3_ halfB);

	/**
	 * @brief 盒与点的碰撞推出（盒为 [A1,A2]×[B1,B2]×[C1,C2]）
	 * @param A1,A2 X 轴最小/最大值
	 * @param B1,B2 Y 轴最小/最大值
	 * @param C1,C2 Z 轴最小/最大值
	 * @param Drop 点位置
	 * @param PY 期望被推出的方向（用于在多个候选面中优选）
	 * @return 点应该移动的距离
	 * @details 2D 版按 PY 分量符号选最近边、比较 PY.x<PY.y 决定归零哪一轴；
	 *          3D 版扩展到三轴，用 |PY| 分量大小排序决定优先推出的轴。 */
	Vec3_ BoxToDrop(FLOAT_ A1, FLOAT_ A2, FLOAT_ B1, FLOAT_ B2,
	                FLOAT_ C1, FLOAT_ C2, Vec3_ Drop, Vec3_ PY);

	/**
	 * @brief 盒与点的碰撞推出（盒中心在原点，半边长 R）
	 * @note 等价于 BoxToDrop(-R, R, -R, R, -R, R, Drop, PY) */
	Vec3_ BoxToDrop(FLOAT_ R, Vec3_ Drop, Vec3_ PY);

	// ───────────────────────── 力矩 / 力分解 ─────────────────────────

	/**
	 * @brief 扭矩（3D 力矩向量）
	 * @param Barycenter 质心位置
	 * @param Spot 受力点位置
	 * @param Force 力向量
	 * @return 返回力矩向量 τ = r × F（r = Spot - Barycenter）
	 * @note 2D 版返回标量 τ = |F|·|r|·sin(Δθ)；3D 直接叉积，无三角函数。 */
	Vec3_ TorqueCalculate(Vec3_ Barycenter, Vec3_ Spot, Vec3_ Force);

	/**
	 * @brief 计算两个向量的夹角（弧度）
	 * @note 用 atan2(|a×b|, a·b) 而非 acos(a·b/(|a||b|))，
	 *       在近平行/反平行时数值更稳（2D 版用 acos）。 */
	FLOAT_ CalculateIncludedAngle(Vec3_ V1, Vec3_ V2);

	/**
	 * @brief 求解分解力（3D：按参考法向直接投影）
	 * @param angle 力臂方向向量（作为参考法向）
	 * @param force 受力向量
	 * @return DecompositionForce3D，包含沿参考法向的垂直分量、
	 *         切向平行分量、以及参考法向本身
	 * @details 与 2D 版的关键差异：2D 内部用 acos 求角再 cos/sin 分解，
	 *          3D 改为 v_par = n̂·(v·n̂)（向量投影）、v_perp = v - v_par。
	 *          数学上等价，但无三角函数、无归一化除零。 */
	DecompositionForce3D CalculateDecompositionForce(Vec3_ angle, Vec3_ force);

	/**
	 * @brief 求解分解力的大小（3D） */
	DecompositionForceVal3D CalculateDecompositionForceVal(Vec3_ angle, Vec3_ force);

	// ───────────────────────── Morton / 空间索引 ─────────────────────────

	/**
	 * @brief 莫顿码（3D Z-order 曲线），64 位
	 * @param x,y,z 坐标（各 ≤ 21 bit，超出会被截断）
	 * @return 返回 63 位交错码，位模式 x0y0z0 x1y1z1 ...
	 * @note 2D 版是 32 位（每轴 ≤16 bit，位模式 x0y0 x1y1）。
	 *       3D 必须用 64 位：每轴 21 bit 已是 64/3 上限，
	 *       而 2D 的 mStorey=12 若直接 3D 化需 36 bit，32 位不够。 */
	uint_fast64_t Morton3D(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z);

	/**
	 * @brief 莫顿码反解（3D）—— 由码还原 x/y/z
	 * @param code Morton3D 产生的码
	 * @param x,y,z 输出坐标
	 * @note 空间索引的"由码找格"路径需要反向解码。 */
	void Morton3DInverse(uint_fast64_t code, uint_fast32_t &x, uint_fast32_t &y, uint_fast32_t &z);

	// ───────────────────────── 点-线距离 ─────────────────────────

	/**
	 * @brief 点到线段的最短距离
	 * @note 2D 版实现有缺陷：用 ModulusLength（平方长度）做投影分母
	 *       （PhysicsBaseCollide.cpp:745/:885/:949 的 `t = Dot(AP,AB)/len`），
	 *       导致投影参数被放大 |AB| 倍。3D 版修正为除以平方长度并夹到 [0,1]。 */
	FLOAT_ DropUptoLineShortes(Vec3_ start, Vec3_ end, Vec3_ drop);

	/**
	 * @brief 点到直线的最短距离的交点（交点可在延长线上） */
	Vec3_ DropLineShortesIntersect(Vec3_ start, Vec3_ end, Vec3_ drop);

	/**
	 * @brief 点到线段的最短距离的交点（交点限制在线段上） */
	Vec3_ DropUptoLineShortesIntersect(Vec3_ start, Vec3_ end, Vec3_ drop);

	// ───────────────────────── 通用小工具 ─────────────────────────

	/**
	 * @brief 将值限制在 [low, high] 范围内 */
	FLOAT_ Clamp(FLOAT_ a, FLOAT_ low, FLOAT_ high);

	/**
	 * @brief 生成 [lo, hi) 范围内的随机浮点数 */
	FLOAT_ inline Random(FLOAT_ lo, FLOAT_ hi)
	{
		FLOAT_ r = (FLOAT_)rand();
		r /= RAND_MAX;
		r = (hi - lo) * r + lo;
		return r;
	}

	// ═══════════════════════════════════════════════════════════════════════
	// 3D 专有几何（2D 版没有对应物，为 SAT 碰撞与流形生成所需）
	// ═══════════════════════════════════════════════════════════════════════

	/**
	 * @brief 轴对齐盒与轴对齐盒的相交测试
	 * @param centerA,halfA 盒 A
	 * @param centerB,halfB 盒 B
	 * @return 相交返回 true */
	bool AABBIntersect(const Vec3_ &centerA, const Vec3_ &halfA,
	                   const Vec3_ &centerB, const Vec3_ &halfB);

	/**
	 * @brief 射线与轴对齐盒求交（slab 方法）
	 * @param origin 射线起点
	 * @param dir 射线方向（不必归一化）
	 * @param boxMin,boxMax 盒的最小/最大角
	 * @param tMin,tMax 输出参数区间
	 * @return 相交返回 true（tMin 可为负，表示起点在盒内） */
	bool RayAABB(const Vec3_ &origin, const Vec3_ &dir,
	             const Vec3_ &boxMin, const Vec3_ &boxMax,
	             FLOAT_ &tMin, FLOAT_ &tMax);

	/**
	 * @brief 分离轴测试（SAT）：返回是否分离，以及最小穿透轴
	 * @param axes 待测轴数组（调用方传入已归一化的候选轴）
	 * @param axisCount 轴数量
	 * @param centerA,halfA,rotA 盒 A 的中心/半尺寸/旋转
	 * @param centerB,halfB,rotB 盒 B 的中心/半尺寸/旋转
	 * @param outNormal 输出的最小穿透轴（世界系单位向量）
	 * @param outDepth 输出的最小穿透深度（正值）
	 * @return 若存在分离轴（即无碰撞）返回 true
	 * @details 对 OBB 需测试 15 条轴（3+3+9 叉积）；对 AABB 只需 3 条。
	 *          2D 引擎完全没有 SAT，靠 Bresenham 点采样近似，3D 必须补上。 */
	bool SATTest(const Vec3_ *axes, int axisCount,
	             const Vec3_ &centerA, const Vec3_ &halfA, const Mat3_ &rotA,
	             const Vec3_ &centerB, const Vec3_ &halfB, const Mat3_ &rotB,
	             Vec3_ &outNormal, FLOAT_ &outDepth);

	/**
	 * @brief 生成 OBB 的 15 条 SAT 候选轴
	 * @param rotA,rotB 两个盒的旋转矩阵
	 * @param out 输出数组（至少 15 个元素）
	 * @param outCount 实际写入数量 */
	void BuildSATAxes(const Mat3_ &rotA, const Mat3_ &rotB, Vec3_ *out, int &outCount);

	/**
	 * @brief 把参考面的多边形裁剪到另一个盒的平面范围内（Sutherland-Hodgman）
	 * @param polyIn 输入多边形顶点（世界系，逆时针）
	 * @param countIn 输入顶点数
	 * @param planeNormal 裁剪平面法向
	 * @param planeD 平面方程 n·p = d
	 * @param polyOut 输出多边形顶点（容量需 >= countIn + 1）
	 * @param countOut 输出顶点数
	 * @details 用于生成 face-clipped 接触流形：先找出参考面，
	 *          再用入射盒的 4 个侧平面依次裁剪，得到精确接触多边形。 */
	void ClipPolygonToPlane(const Vec3_ *polyIn, int countIn,
	                        const Vec3_ &planeNormal, FLOAT_ planeD,
	                        Vec3_ *polyOut, int &countOut);

	/**
	 * @brief 计算多边形的近似面积（用于按面积选参考面）
	 * @param poly 多边形顶点（世界系）
	 * @param count 顶点数
	 * @param normal 多边形法向
	 * @return 面积 */
	FLOAT_ PolygonArea(const Vec3_ *poly, int count, const Vec3_ &normal);

	/**
	 * @brief 从法向量构造一组正交切向基（用于摩擦求解）
	 * @param normal 单位法向
	 * @param tangent1,tangent2 输出的两个正交单位切向
	 * @details 2D 只需一个切向 (n.y,-n.x)；3D 需要两个正交切向，
	 *          摩擦从"1D 钳制"变为"圆盘投影"（F ≤ μ·N）。 */
	void BuildTangentBasis(const Vec3_ &normal, Vec3_ &tangent1, Vec3_ &tangent2);

	/**
	 * @brief 把切向冲量投影到摩擦圆盘内（3D 摩擦锥）
	 * @param tangentImpulse 切向冲量（二维，在 tangent1/tangent2 基上）
	 * @param maxFriction 最大摩擦力 μ·N
	 * @return 投影后的切向冲量 */
	Vec3_ ProjectFrictionDisk(const Vec3_ &tangentImpulse, FLOAT_ maxFriction);

	/**
	 * @brief 把 3×3 矩阵按对称正定求逆（比 glm::inverse 快且更稳）
	 * @return 若矩阵奇异返回零矩阵 */
	Mat3_ SymmetricInverse(const Mat3_ &m);

}
