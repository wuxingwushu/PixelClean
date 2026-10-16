#pragma once
#include "BaseGrid3D.hpp"
#include <vector>

namespace PhysicsBlock3D
{

	/**
	 * @brief 三维轮廓点
	 * @details 由 2D 的"角点 + 贪婪矩形中心"三元组改写而来。
	 *
	 * 2D 语义回顾（BaseOutline.hpp:19-34）：
	 *   OutlineSet[i]            —— 形状边界的角点（局部坐标）
	 *   MaxOutlineCentreMass[i]  —— 包含该角点的最大贪婪矩形的中心
	 *   FrictionSet[i]           —— 该点的摩擦因数
	 * 窄相（PhysicsBaseCollide.cpp:174-223）从 MaxOutlineCentreMass 向 OutlineSet
	 * 打 Bresenham 射线，由命中格的轴向反推碰撞法线。
	 *
	 * 3D 的关键改进：直接枚举"表面体素的外向面"，法线是几何已知量，
	 * 不再需要"打射线 + 反推轴向"。因此 LocalPos 取面中心而非角点，
	 * 并新增 Normal 字段直接携带外向法线。
	 *
	 * 内存设计：2D 用固定数组 width*height*3，最坏 4*W*H 会越界
	 * （BaseOutline.cpp:14-23 的既有缺陷）。3D 若照搬需 W*H*D*8，
	 * 256³ 最坏 1.34e8 项 ≈3.8GB，完全不可行。故改为
	 * 表面体素驱动的动态数组，规模与表面积成正比（6*N² 量级）。
	 */
	struct OutlinePoint3D
	{
		/** @brief 表面点局部坐标（相对质心，位于体素面中心） */
		Vec3_ LocalPos;

		/** @brief 外向面法线（局部坐标系，6 个轴向之一） */
		Vec3_ Normal;

		/** @brief 该点的摩擦因数 */
		FLOAT_ Friction;

		/** @brief 射线种子点（形状内部，对应 2D 的 MaxOutlineCentreMass） */
		Vec3_ SeedPos;

		/** @brief 表面点所在体素坐标（用于去重与调试） */
		IVec3_ Voxel;

		OutlinePoint3D()
		    : LocalPos(0.0f, 0.0f, 0.0f), Normal(0.0f, 1.0f, 0.0f),
		      Friction(0.2f), SeedPos(0.0f, 0.0f, 0.0f), Voxel(0, 0, 0) {}

		OutlinePoint3D(Vec3_ localPos, Vec3_ normal, FLOAT_ friction, Vec3_ seed, IVec3_ voxel)
		    : LocalPos(localPos), Normal(normal), Friction(friction), SeedPos(seed), Voxel(voxel) {}
	};

	/**
	 * @brief 三维外轮廓类
	 * @details 继承自 BaseGrid3D，在体素网格上提取表面轮廓。
	 *
	 * 与 2D 版的三处结构性差异：
	 *  1. 用 std::vector<OutlinePoint3D> 替代三条平行裸数组，
	 *     根治 2D 的越界隐患（OutlineUnit 单格最多推 4 角点，
	 *     但数组只按 3*W*H 分配）与 MaxOutlineCentreMass 漏分配问题。
	 *  2. 用"表面体素 + 外向面法线"替代"角点 + 贪婪矩形中心"，
	 *     法线几何已知，窄相无需再反推。
	 *  3. 提供 Clear/Reserve 以支持稳态零分配（对齐 GridSearch 的设计目标）。
	 */
	class BaseOutline3D : public BaseGrid3D
	{
#if PhysicsBlock_Serialization3D
	public:
		/**
		 * @brief 序列化虚函数
		 */
		SerializationVirtualFunction3D;
#endif
	public:
		/**
		 * @brief 轮廓点集合
		 */
		std::vector<OutlinePoint3D> OutlineSet;

		/**
		 * @brief 轮廓点数量（= OutlineSet.size()，保留 2D 命名便于移植）
		 */
		unsigned int OutlineSize = 0;

		/**
		 * @brief 轮廓包围半径（相对质心的最大距离，供宽相快速剔除）
		 */
		FLOAT_ CollisionR = 0.0f;

#if PhysicsBlock_Serialization3D
		/**
		 * @brief 从 JSON 数据构造
		 */
		BaseOutline3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data) : BaseGrid3D(data)
		{
			JsonContrarySerialization(data);
		}
#endif

		/**
		 * @brief 构造函数
		 */
		BaseOutline3D(const unsigned int Width, const unsigned int Height, const unsigned int Depth);

		/**
		 * @brief 析构函数
		 */
		~BaseOutline3D();

		/**
		 * @brief 更新外轮廓点集合（完整版）
		 * @param CentreMass_ 质心位置（局部体素坐标）
		 * @details 枚举所有实体体素的 6 个面，邻居为空（或越界）则记录为表面点。
		 *          最后把坐标平移到相对质心。
		 */
		void UpdateOutline(Vec3_ CentreMass_);

		/**
		 * @brief 更新外轮廓点集合（轻量版）
		 * @param CentreMass_ 质心位置
		 * @details 在完整版基础上做面合并：同一法线方向上的相邻共面面片
		 *          只保留其中心点，点数约降到 1/(边长) 量级。
		 *          对应 2D 的 LightweightOutlineUnit（更严格的角点条件）。
		 */
		void UpdateLightweightOutline(Vec3_ CentreMass_);

		/**
		 * @brief 更新外轮廓点集合（最小版）
		 * @param CentreMass_ 质心位置
		 * @details 只保留每个轴向极值附近的表面点（6 个方向的代表点），
		 *          点数最少，用于远距离/低精度碰撞。
		 *          对应 2D 的 MinOutlineUnit。
		 */
		void UpdateMinOutline(Vec3_ CentreMass_);

		/**
		 * @brief 清空轮廓
		 */
		void Clear()
		{
			OutlineSet.clear();
			OutlineSize = 0;
			CollisionR = 0.0f;
		}

		/**
		 * @brief 预留容量（避免稳态下反复分配）
		 */
		void Reserve(size_t n) { OutlineSet.reserve(n); }

		/**
		 * @brief 重新计算轮廓包围半径
		 */
		void UpdateCollisionR();

	private:
		/**
		 * @brief 表面点收集内部实现
		 * @param mode 0=完整 1=轻量 2=最小
		 * @param CentreMass_ 质心位置
		 */
		void BuildOutline(int mode, Vec3_ CentreMass_);

		/**
		 * @brief 取种子点：形状内部最靠近该表面点的实体体素中心
		 * @details 对应 2D 的 MaxOutlineCentre（最大贪婪矩形中心）。
		 *          3D 用"沿法线反向找到最深实体体素"替代贪婪矩形，
		 *          语义等价（都是从内部向表面打射线的起点）但 O(深度) 而非 O(N²)。
		 */
		Vec3_ FindSeedPoint(IVec3_ surfaceVoxel, IVec3_ normal) const;

		/**
		 * @brief 体素是否为实体（越界视为空）
		 */
		inline bool IsSolid(int x, int y, int z) const
		{
			if (!InBounds(x, y, z))
			{
				return false;
			}
			return GridAt(x, y, z).Entity;
		}
	};

}
