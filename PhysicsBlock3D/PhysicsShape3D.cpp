#include "PhysicsShape3D.hpp"
#include "BaseCalculate3D.hpp"

namespace PhysicsBlock3D
{

	/**
	 * @brief 构造函数
	 * @param Pos 形状的初始位置
	 * @param Size 网格大小（三维）
	 */
	PhysicsShape3D::PhysicsShape3D(Vec3_ Pos, IVec3_ Size)
	    : PhysicsAngle3D(Pos), BaseOutline3D(Size.x, Size.y, Size.z)
	{
		type = PhysicsObjectEnum::shape;
	}

	PhysicsShape3D::~PhysicsShape3D()
	{
	}

	/**
	 * @brief 检测点是否点击到网格
	 * @param Pos 点的世界位置
	 * @return 碰撞结果信息
	 * @details 1. 世界坐标 → 形状局部坐标
	 *          2. 用四元数逆旋转把点转到未旋转的网格坐标系
	 *          3. 计算体素坐标并 clamp 到 [0, size-1]
	 *          4. 查询 Collision 标志
	 */
	CollisionInfoI3D PhysicsShape3D::DropCollision(Vec3_ Pos)
	{
		Pos -= pos;                                    // 转换为局部坐标
		Pos = QuatInverseRotate(rotation, Pos);        // 反向旋转到网格坐标系
		CollisionInfoI3D info;
		info.pos = ToInt(Pos + CentreMass);            // 计算体素坐标

		// 处理边界情况：接触点恰好在边界时体素坐标会等于 size，需 clamp 到有效范围
		if (info.pos.x >= static_cast<int>(width))
		{
			info.pos.x = static_cast<int>(width) - 1;
		}
		if (info.pos.y >= static_cast<int>(height))
		{
			info.pos.y = static_cast<int>(height) - 1;
		}
		if (info.pos.z >= static_cast<int>(depth))
		{
			info.pos.z = static_cast<int>(depth) - 1;
		}
		// 负坐标同样需要 clamp（2D 版只处理了上界）
		if (info.pos.x < 0)
		{
			info.pos.x = 0;
		}
		if (info.pos.y < 0)
		{
			info.pos.y = 0;
		}
		if (info.pos.z < 0)
		{
			info.pos.z = 0;
		}

		info.Collision = GetCollision(static_cast<unsigned int>(info.pos.x),
		                              static_cast<unsigned int>(info.pos.y),
		                              static_cast<unsigned int>(info.pos.z));
		return info;
	}

	/**
	 * @brief 更新形状的物理信息
	 * @details 计算总质量、几何中心、质心与 3×3 转动惯量张量。
	 *
	 * 3D 惯量张量公式（对每个采样点 r，质量为 m）：
	 *   I += m * ( (r·r)·E − r⊗r )
	 * 即
	 *   Ixx = m(r_y²+r_z²), Iyy = m(r_x²+r_z²), Izz = m(r_x²+r_y²)
	 *   Ixy = −m·r_x·r_y,   Ixz = −m·r_x·r_z,   Iyz = −m·r_y·r_z
	 * 2D 只保留标量 Izz（绕 Z 轴），3D 必须给出完整张量，否则绕 X/Y 轴
	 * 的旋转会完全错误。
	 */
	void PhysicsShape3D::UpdateInfo()
	{
		const Vec3_ UsedCentreMass = CentreMass; // 旧质心
		mass = 0;
		unsigned int Size = 0;                       // 实体格子数
		CentreMass = Vec3_(0.0f);                    // 清空位置
		CentreShape = Vec3_(0.0f);                   // 清空位置

		// 计算质量、几何中心和质心
		for (int x = 0; x < static_cast<int>(width); ++x)
		{
			for (int y = 0; y < static_cast<int>(height); ++y)
			{
				for (int z = 0; z < static_cast<int>(depth); ++z)
				{
					const unsigned int i = (x * height + y) * depth + z;
					if (!at(i).Entity)
					{
						continue;
					}
					mass += at(i).mass;
					++Size;
					CentreShape += Vec3_(static_cast<FLOAT_>(x), static_cast<FLOAT_>(y), static_cast<FLOAT_>(z));
					CentreMass += Vec3_(static_cast<FLOAT_>(x), static_cast<FLOAT_>(y), static_cast<FLOAT_>(z)) * at(i).mass;
				}
			}
		}

		if (Size > 0)
		{
			CentreShape /= static_cast<FLOAT_>(Size);
		}
		invMass = (mass > 0.0f) ? (1.0f / mass) : 0.0f;

		// 处理质量为零的情况
		if (invMass == 0)
		{
			mass = FLOAT_MAX;
			CentreMass = CentreShape;
		}
		else
		{
			CentreMass /= mass;
		}

		CentreShape += Vec3_(0.5f, 0.5f, 0.5f); // 移动到格子的中心
		CentreMass += Vec3_(0.5f, 0.5f, 0.5f);  // 移动到格子的中心

		// 位置指质心在世界坐标的位置，质心改了，位置也需要按当前朝向做偏移
		pos += QuatRotate(rotation, CentreMass - UsedCentreMass);
		OldPos = pos;

		// ── 计算转动惯量张量 ──
		MomentInertia = Mat3_(0.0f);
		const FLOAT_ sampleCount = static_cast<FLOAT_>(MomentInertiaSamplingSize3D) *
		                           static_cast<FLOAT_>(MomentInertiaSamplingSize3D) *
		                           static_cast<FLOAT_>(MomentInertiaSamplingSize3D);
		const FLOAT_ invSample = 1.0f / static_cast<FLOAT_>(MomentInertiaSamplingSize3D);
		const FLOAT_ halfSample = 0.5f * invSample;

		for (int x = 0; x < static_cast<int>(width); ++x)
		{
			for (int y = 0; y < static_cast<int>(height); ++y)
			{
				for (int z = 0; z < static_cast<int>(depth); ++z)
				{
					const unsigned int i = (x * height + y) * depth + z;
					if (!at(i).Entity)
					{
						continue;
					}
					const FLOAT_ Lmass = at(i).mass / sampleCount;

					// 三维采样：MomentInertiaSamplingSize3D³ 个子采样点
					for (int cx = 0; cx < MomentInertiaSamplingSize3D; ++cx)
					{
						for (int cy = 0; cy < MomentInertiaSamplingSize3D; ++cy)
						{
							for (int cz = 0; cz < MomentInertiaSamplingSize3D; ++cz)
							{
								Vec3_ lpos(static_cast<FLOAT_>(x) + invSample * cx + halfSample,
								           static_cast<FLOAT_>(y) + invSample * cy + halfSample,
								           static_cast<FLOAT_>(z) + invSample * cz + halfSample);
								lpos -= CentreMass; // 相对质心

								const FLOAT_ r2 = lpos.x * lpos.x + lpos.y * lpos.y + lpos.z * lpos.z;
								// I = Σ m (|r|²E − r⊗r)
								MomentInertia[0][0] += Lmass * (r2 - lpos.x * lpos.x);
								MomentInertia[1][1] += Lmass * (r2 - lpos.y * lpos.y);
								MomentInertia[2][2] += Lmass * (r2 - lpos.z * lpos.z);
								MomentInertia[0][1] -= Lmass * lpos.x * lpos.y;
								MomentInertia[1][0] -= Lmass * lpos.x * lpos.y;
								MomentInertia[0][2] -= Lmass * lpos.x * lpos.z;
								MomentInertia[2][0] -= Lmass * lpos.x * lpos.z;
								MomentInertia[1][2] -= Lmass * lpos.y * lpos.z;
								MomentInertia[2][1] -= Lmass * lpos.y * lpos.z;
							}
						}
					}
				}
			}
		}

		invMomentInertia = SymmetricInverse(MomentInertia);
		// 奇异张量（质量为零）视作无限惯量
		if (invMomentInertia == Mat3_(0.0f))
		{
			MomentInertia = Mat3_(FLOAT_MAX);
		}
	}

	/**
	 * @brief 计算碰撞半径
	 * @details 从质心到轮廓上最远点的距离（取距离平方的最大值再开方）
	 */
	void PhysicsShape3D::UpdateCollisionR()
	{
		radius = 0.0f;
		for (const auto &p : OutlineSet)
		{
			const FLOAT_ R = ModulusLength(p.LocalPos);
			if (radius < R)
			{
				radius = R;
			}
		}
		radius = SQRT_(radius);
		CollisionR = radius;
	}

	/**
	 * @brief 更新全部信息
	 */
	void PhysicsShape3D::UpdateAll()
	{
		if (mass == FLOAT_MAX)
		{
			invMass = 0;
			MomentInertia = Mat3_(FLOAT_MAX);
			invMomentInertia = Mat3_(0.0f);
			CentreMass = Vec3_(static_cast<FLOAT_>(width) * 0.5f,
			                   static_cast<FLOAT_>(height) * 0.5f,
			                   static_cast<FLOAT_>(depth) * 0.5f);
			CentreShape = CentreMass;
			OldPos = pos;
		}
		else
		{
			UpdateInfo();
		}
		UpdateMinOutline(CentreMass);
		UpdateCollisionR();
	}

	/**
	 * @brief 几何形状靠近指定点
	 * @param drop 目标点的位置
	 * @details 从形状中心沿"指向 drop"的方向穿过形状打一条线段（局部网格坐标），
	 *          DDA 找到最先命中的表面点，再平移形状使该表面点落在 drop 上。
	 * @note 2D 版把局部网格命中点直接当作世界点相加（`pos += drop - (info.pos + pos)`），
	 *       未考虑旋转。3D 用四元数把命中点正确变换回世界坐标。
	 */
	void PhysicsShape3D::ApproachDrop(Vec3_ drop)
	{
		const Vec3_ dir = drop - pos;
		if (ModulusLength(dir) <= FLOAT_(1e-12))
		{
			return;
		}
		// 局部网格空间中的线段：穿过质心，沿朝向 drop 的方向，半长 = radius
		const Vec3_ localDir = QuatInverseRotate(rotation, SafeNormalize(dir) * radius);
		const CollisionInfoD3D info = DDADetection(CentreMass + localDir, CentreMass - localDir);
		if (!info.Collision)
		{
			return;
		}
		// 命中点：局部网格坐标 → 世界坐标
		const Vec3_ worldHit = pos + QuatRotate(rotation, info.pos);
		pos += drop - worldHit;
		OldPos = pos;
	}

	/**
	 * @brief 射线碰撞检测
	 * @param Pos 射线起点
	 * @param Dir 射线方向向量
	 * @details 以 Pos 为中心、沿 Dir 方向、半长 radius 的线段做碰撞检测。
	 * @note 2D 版为 `RayCollide(Vec2_ Pos, FLOAT_ Angle)`，3D 用方向向量。
	 */
	CollisionInfoD3D PhysicsShape3D::RayCollide(Vec3_ Pos, Vec3_ Dir)
	{
		const FLOAT_ len = glm::length(Dir);
		if (len <= FLOAT_(1e-12))
		{
			return CollisionInfoD3D{};
		}
		const Vec3_ drop = (Dir / len) * radius;
		return PsDDADetection(Pos - drop, Pos + drop);
	}

	/**
	 * @brief 线段碰撞检测（整数体素坐标）
	 */
	CollisionInfoI3D PhysicsShape3D::PsDDADetection(IVec3_ start, IVec3_ end)
	{
		return DDADetection(start, end);
	}

	/**
	 * @brief 线段碰撞检测（浮点世界坐标）
	 * @details 1. 线段转到形状局部坐标
	 *          2. 用四元数逆旋转转到未旋转的网格坐标系
	 *          3. 裁剪线段到网格包围盒内
	 *          4. 三维 DDA 检测
	 *          5. 命中点与法线变换回世界坐标系
	 */
	CollisionInfoD3D PhysicsShape3D::PsDDADetection(Vec3_ start, Vec3_ end)
	{
		// 偏移到网格坐标系
		start -= pos;
		end -= pos;

		// 逆旋转（2D 的 AngleMat::Anticlockwise 对应物）
		start = QuatInverseRotate(rotation, start);
		end = QuatInverseRotate(rotation, end);

		// 裁剪线段，让线段都在盒内
		const BoxFocus data = LineBoxFocus(start + CentreMass, end + CentreMass,
		                                   Vec3_(static_cast<FLOAT_>(width),
		                                         static_cast<FLOAT_>(height),
		                                         static_cast<FLOAT_>(depth)));
		if (data.Focus)
		{
			CollisionInfoD3D info = DDADetection(data.start, data.end);
			if (info.Collision)
			{
				// 返回物理坐标系：命中点旋转 + 平移
				info.pos = QuatRotate(rotation, info.pos - CentreMass) + pos;
				// 入射面法线也要跟着旋转，Direction 由世界法线重新编码
				info.Direction = DirectionFromNormal(QuatRotate(rotation, DirectionNormal(info.Direction)));
				return info;
			}
		}
		return CollisionInfoD3D{};
	}

#if PhysicsBlock_Serialization3D
	/**
	 * @brief 将形状状态序列化为 JSON
	 */
	void PhysicsShape3D::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
	{
		PhysicsAngle3D::JsonSerialization(data);
		BaseOutline3D::JsonSerialization(data);
	}

	/**
	 * @brief 从 JSON 数据反序列化形状状态
	 */
	void PhysicsShape3D::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
	{
		PhysicsAngle3D::JsonContrarySerialization(data);
		BaseOutline3D::JsonContrarySerialization(data);
		UpdateAll();
		ContrarySerializationVec3(data, pos);
		ContrarySerializationVec3(data, OldPos);
	}
#endif

}
