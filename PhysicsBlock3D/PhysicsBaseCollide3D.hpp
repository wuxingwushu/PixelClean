#pragma once
#include "PhysicsParticle3D.hpp"
#include "PhysicsShape3D.hpp"
#include "PhysicsCircle3D.hpp"
#include "PhysicsLine3D.hpp"
#include "MapFormwork3D.hpp"
#include "BaseArbiter3D.hpp"

namespace PhysicsBlock3D
{

	// =========================================================================
	// 宽相：AABB 粗筛（7 个重载）
	// =========================================================================
	//
	// 2D 版（PhysicsBaseCollide.cpp:16-154）全部是「中心距 vs 半径和」且**严格 2 轴**：
	//   if (abs(A.pos.x - B.pos.x) > R) return false;
	//   if (abs(A.pos.y - B.pos.y) > R) return false;
	// 3D 必须补上 Z 轴比较，否则所有物体在 Z 方向无限厚，宽相形同虚设。
	//
	// 2D 的 Line 两个重载是唯一考虑朝向的（用 vec2angle({radius,0}, angle) 求
	// 线段的轴对齐包围盒半长）；3D 改为 QuatRotate(rotation, Vec3_(radius,0,0))
	// 后取三个分量的绝对值。

	bool CollideAABB(PhysicsShape3D *ShapeA, PhysicsShape3D *ShapeB);
	bool CollideAABB(PhysicsShape3D *Shape, PhysicsParticle3D *Particle);
	bool CollideAABB(PhysicsShape3D *Shape, PhysicsCircle3D *Circle);
	bool CollideAABB(PhysicsCircle3D *CircleA, PhysicsCircle3D *CircleB);
	bool CollideAABB(PhysicsCircle3D *Circle, PhysicsParticle3D *Particle);
	bool CollideAABB(PhysicsLine3D *Line, PhysicsShape3D *Shape);
	bool CollideAABB(PhysicsLine3D *Line, PhysicsCircle3D *Circle);

	// =========================================================================
	// 窄相：精确接触点生成（12 个重载）
	// =========================================================================
	//
	// 【法线方向约定（关键，必须与 PhysicsBaseArbiter3D 自洽）】
	//   PhysicsBaseArbiter3D.cpp 的求解器按
	//       dv = object2->speed + ω2×r2 − object1->speed − ω1×r1
	//       vn = Dot(dv, normal)
	//       dPn = massNormal * (−vn + bias)
	//       object1 −= invMass*Pn ; object2 += invMass*Pn
	//   计算。故 **normal 必须由 object1 指向 object2**。
	//
	// 【3D 与 2D 在法线推导上的本质差异】
	//   2D：`normal = vec2angle({-1,0}, info.Direction*(π/2) + Shape->angle)`
	//       —— Direction 只有 2 bit（4 向），且需要用「方向位 × 90° + 标量角」
	//          反推，还要区分是否叠加物体自身的 angle。
	//   3D：`DirectionNormal(info.Direction)` 直接给出**命中面的外向单位法向**
	//       （BaseGrid3D 的 FaceFromStep 已保证语义），且 PhysicsShape3D::PsDDADetection
	//       已把 Direction 旋转回世界系。所以法线是几何已知量，无需反推。
	//
	//   设 n_out = DirectionNormal(info.Direction)（世界系，命中面的外向法向）：
	//     · 命中 ShapeB 的面 → n_out 指向 B→A，故 normal(A→B) = −n_out
	//     · 命中 ShapeA 的面 → n_out 指向 A→B，故 normal(A→B) = +n_out
	//     · 命中地图的面   → n_out 指向 地图→物体，故 normal(物体→地图) = −n_out
	//     · 命中 Shape 的面（对方是 particle/circle）→ n_out 指向 Shape→对方
	//         · 若 object1 = Shape（如 SP）→ normal = +n_out
	//         · 若 object2 = Shape（如 CS）→ normal = −n_out
	//
	// 【separation 符号】
	//   沿用 2D 的稳健做法：`separation = -abs(沿法向的投影)`，恒 ≤ 0（负值=穿透）。
	//   这样即使射线是从内部射出（物体正在离开）也不会产生"正分离"导致的抖动。

	unsigned int Collide(Contact3D *contacts, PhysicsShape3D *ShapeA, PhysicsShape3D *ShapeB);
	unsigned int Collide(Contact3D *contacts, PhysicsShape3D *Shape, PhysicsParticle3D *Particle);
	unsigned int Collide(Contact3D *contacts, PhysicsShape3D *Shape, MapFormwork3D *Map);
	unsigned int Collide(Contact3D *contacts, PhysicsParticle3D *Particle, MapFormwork3D *Map);
	unsigned int Collide(Contact3D *contacts, PhysicsCircle3D *Circle, MapFormwork3D *Map);
	unsigned int Collide(Contact3D *contacts, PhysicsCircle3D *Circle, PhysicsParticle3D *Particle);
	unsigned int Collide(Contact3D *contacts, PhysicsCircle3D *CircleA, PhysicsCircle3D *CircleB);
	unsigned int Collide(Contact3D *contacts, PhysicsCircle3D *Circle, PhysicsShape3D *Shape);
	unsigned int Collide(Contact3D *contacts, PhysicsLine3D *Line, PhysicsShape3D *Shape);
	unsigned int Collide(Contact3D *contacts, PhysicsLine3D *Line, PhysicsCircle3D *Circle);
	unsigned int Collide(Contact3D *contacts, PhysicsLine3D *Line, PhysicsParticle3D *Particle);
	unsigned int Collide(Contact3D *contacts, PhysicsLine3D *Line, MapFormwork3D *Map);

	// =========================================================================
	// 3D 专有几何辅助（2D 没有对应物）
	// =========================================================================

	/**
	 * @brief 两线段最近点（Ericson《Real-Time Collision Detection》§5.1.9）
	 * @details 2D 有 segmentIntersection（两线段必有交点或平行），3D 中两异面直线
	 *          一般不相交，故必须改为「最近点对 + 距离」。
	 *          这是 Line-Shape / Line-Particle 在 3D 的基础。
	 * @param p1,q1 第一条线段端点
	 * @param p2,q2 第二条线段端点
	 * @param c1 输出第一条线段上的最近点
	 * @param c2 输出第二条线段上的最近点
	 * @param dist 输出最近距离
	 * @return 两线段是否相交/重叠（dist ≤ epsilon）
	 */
	bool ClosestSegmentSegment(const Vec3_ &p1, const Vec3_ &q1,
	                           const Vec3_ &p2, const Vec3_ &q2,
	                           Vec3_ &c1, Vec3_ &c2, FLOAT_ &dist);

	/**
	 * @brief 接触点流形归约
	 * @details 2D 只是用 `ContactSize < PhysicsContactMaxSize` 按遍历顺序截断
	 *          （PhysicsBaseCollide.cpp:174/206 等），最多 20 点且无几何筛选。
	 *          3D 接触点更多（面 vs 面），故先按穿透深度降序，再贪心选取
	 *          「与已选点距离最远」的点，归约到 PhysicsContactManifoldMaxSize3D(4) 个，
	 *          保证流形覆盖接触面而不是聚集在一角。
	 * @param contacts 接触点数组（原地修改）
	 * @param count 输入/输出接触点数量
	 */
	void ReduceManifold(Contact3D *contacts, unsigned int &count);

	/**
	 * @brief 由体素坐标 + 面法向生成稳定特征 ID
	 * @details 3D 的 w_side 语义从 2D 的「下标」改为「特征 ID」，用于跨帧匹配
	 *          接触点以继承累积冲量（PhysicsBaseArbiterAA3D::Update）。
	 *          用体素坐标与面索引哈希，保证同一几何特征在相邻帧得到同一 ID。
	 */
	unsigned char FeatureId(IVec3_ voxel, unsigned int faceIndex);

}
