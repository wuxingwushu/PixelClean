#pragma once
#include "BaseGrid3D.hpp"
#include <vector>

namespace PhysicsBlock3D
{

	/**
	 * @brief 三维轮廓点（方形面的顶点）
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
	 * 不再需要"打射线 + 反推轴向"，并新增 Normal 字段直接携带外向法线。
	 *
	 * 取点语义（与 2D 的角点对齐）：LocalPos 取方形面的**顶点**（局部网格整数
	 * 坐标），不是面中心。面中心会丢掉整个面的朝向信息 —— 16×16 的大平面只剩
	 * 1 个采样点，接触流形退化成单点；取 4 个顶点后射线终点铺满面的四角，
	 * 流形才能真正张开，顶点坐标也能直接用于绘制、凸包与调试。
	 *
	 * 顶点顺序：每连续 4 个点为同一个方形面，下标 0..3 沿面环绕（右手法则，
	 * 外法线为正面朝向）：(p1-p0)×(p2-p1) 与 Normal 同向，故 0-1-2-3 直接就是
	 * 一个朝向正确的四边形。Corner 字段保存该序号，使 4 个点能绑定回同一个面
	 * （窄相特征 ID、调试绘制、序列化都依赖它）。
	 *
	 * SeedPos 是该点射线的起点，即 2D 的 MaxOutlineCentreMass：**包含该体素的
	 * 最大内接轴对齐长方体的中心**（沿面法线单向深入在 3D 不成立 —— 3×3×3 实心
	 * 立方会得到 9 个不同的"质心"，而正确结果只有一个立方体中心）。同一体素的
	 * 6 个面共用这个质心；方形面的同一个顶点被相邻 2~3 个面各输出一次时，副本
	 * 再统一取离该顶点最近的那个质心。所以同一个可见的外骨骼点永远只连出一条
	 * 射线（SeedPos → LocalPos，终点落在面上），可视化与碰撞射线是同一条。
	 *
	 * 内存设计：2D 用固定数组 width*height*3，最坏 4*W*H 会越界
	 * （BaseOutline.cpp:14-23 的既有缺陷）。3D 若照搬需 W*H*D*8，
	 * 256³ 最坏 1.34e8 项 ≈3.8GB，完全不可行。故改为
	 * 表面体素驱动的动态数组，规模与表面积成正比（24*N² 量级）。
	 */
	struct OutlinePoint3D
	{
		/** @brief 面顶点局部坐标（相对质心，落在局部网格整数点上） */
		Vec3_ LocalPos;

		/** @brief 外向面法线（局部坐标系，6 个轴向之一） */
		Vec3_ Normal;

		/** @brief 该点的摩擦因数 */
		FLOAT_ Friction;

		/** @brief 射线起点：包含该体素的最大内接长方体的中心（2D MaxOutlineCentreMass 的 3D 版） */
		Vec3_ SeedPos;

		/** @brief 顶点所在表面体素坐标（用于去重、调试与热启动特征） */
		IVec3_ Voxel;

		/** @brief 顶点在方形面内的序号 0..3（从面外侧看逆时针） */
		unsigned char Corner;

		OutlinePoint3D()
		    : LocalPos(0.0f, 0.0f, 0.0f), Normal(0.0f, 1.0f, 0.0f),
		      Friction(0.2f), SeedPos(0.0f, 0.0f, 0.0f), Voxel(0, 0, 0), Corner(0) {}

		OutlinePoint3D(Vec3_ localPos, Vec3_ normal, FLOAT_ friction, Vec3_ seed,
		               IVec3_ voxel, unsigned char corner)
		    : LocalPos(localPos), Normal(normal), Friction(friction), SeedPos(seed),
		      Voxel(voxel), Corner(corner) {}
	};

	/**
	 * @brief 三维外轮廓类
	 * @details 继承自 BaseGrid3D，在体素网格上提取表面轮廓。
	 *
	 * 与 2D 版的三处结构性差异：
	 *  1. 用 std::vector<OutlinePoint3D> 替代三条平行裸数组，
	 *     根治 2D 的越界隐患（OutlineUnit 单格最多推 4 角点，
	 *     但数组只按 3*W*H 分配）与 MaxOutlineCentreMass 漏分配问题。
	 *  2. 用"表面体素 + 外向面法线"替代"角点 + 边"，法线几何由网格直接给出，
	 *     窄相无需再反推；每个暴露面输出其 4 个顶点，与 2D 的角点语义对齐
	 *     （2D 的 MinOutlineUnit 同样取格子角点）。质心仍沿用 2D 的
	 *     MaxOutlineCentre 定义，只是从"最大内接矩形"推广到"最大内接长方体"。
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
		 * @details 枚举所有实体体素的 6 个面，邻居为空（或越界）则把该方形面的
		 *          **4 个顶点**全部记录（Corner 依次为 0..3），点数 = 4 × 暴露面数。
		 *          同一顶点被相邻面重复输出时，其副本统一改用离顶点最近的 SeedPos。
		 *          最后把坐标平移到相对质心。
		 */
		void UpdateOutline(Vec3_ CentreMass_);

		/**
		 * @brief 更新外轮廓点集合（轻量版）
		 * @param CentreMass_ 质心位置
		 * @details 在完整版基础上做面合并：同一法线方向上的相邻共面面片合并成
		 *          一条条带，每个条带只输出其矩形的 4 个顶点（仍是方形面的顶点，
		 *          仍按 Corner 0..3 环绕），点数约降到 1/(边长) 量级。
		 *          对应 2D 的 LightweightOutlineUnit（更严格的角点条件）。
		 */
		void UpdateLightweightOutline(Vec3_ CentreMass_);

		/**
		 * @brief 更新外轮廓点集合（最小版）
		 * @param CentreMass_ 质心位置
		 * @details 每个法线方向只保留 4 个支撑顶点：按面内两条切向轴的 4 个对角
		 *          方向 (u+v, u-v, -u+v, -u-v) 各取极值面，再取该面上对应角的
		 *          **顶点**（不是面中心）。点数最少（最多 24），用于远距离/低精度
		 *          碰撞 —— 运行期 UpdateAll 即走此版本。对应 2D 的 MinOutlineUnit。
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
		 * @details 内部会为整个网格建一张实心前缀和，用于计算
		 *          「最大外骨骼点质心」= 包含该体素的最大内接长方体中心
		 *          （2D 的 MaxOutlineCentre 就是这个定义，不依赖面法线）。
		 */
		void BuildOutline(int mode, Vec3_ CentreMass_);

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
