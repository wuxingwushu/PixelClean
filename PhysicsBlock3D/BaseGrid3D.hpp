#pragma once
#include "BaseStruct3D.hpp"
#include "BaseSerialization3D.hpp"
#include "BaseCalculate3D.hpp"

// ═══════════════════════════════════════════════════════════════════════════
// 线段碰撞检测算法选择（编译期开关）
// ═══════════════════════════════════════════════════════════════════════════
//   1 = Amanatides-Woo 三维 DDA（默认）
//       参数 t 比较推进，天然支持任意轴数；推进时直接记录"穿过的是哪个轴的面"，
//       精确入射点 = start + dir * tEnter，无需回切推断。
//   0 = 三维 Bresenham（整数步进，与 2D 版同构）
//       纯整数误差累积（errA/errB 双误差项），无浮点累积误差、无乘除法；
//       但只给出"经过哪些体素"，精确入射点与入射面要在命中后再用射线与
//       该体素 AABB 求交反推（见 BaseGrid3D.cpp 的 RayVoxelEntry）。
//
// 两者对外接口完全一致（都返回 CollisionInfoI3D / CollisionInfoD3D），
// 所有调用点（PsDDADetection / FMDDADetection ...）不需要任何改动。
//
// ── 实测对比（24³ 网格 + 地板/墙/柱子，随机线段，见 build/test_lines3d.cpp）──
//   命中体素与"连续几何真值"（0.005 步长密集采样）的一致率：
//     随机三维线段   DDA 99.84%   Bresenham 53.12%
//     平面内线段     DDA 100.00%  Bresenham 68.84%
//     轴对齐线段     DDA 100.00%  Bresenham 100.00%
//   性能（20 万条）：DDA 0.207 us/条，Bresenham 0.101 us/条（约 2 倍快）。
//   Bresenham 偏差经独立"四舍五入参考实现"交叉验证：两者路径最大偏差均为
//   0.516 体素、最坏 0.71 体素（< 1 格），说明实现无误——差异来自数字直线
//   本身会"切角"（斜穿拐角时跳过被擦到的格子），因此斜向薄墙可能漏检。
//   物理引擎选默认值时以正确性优先，故默认 1（DDA）。
#ifndef PhysicsBlock_LineTrace3D
#define PhysicsBlock_LineTrace3D 1
#endif

namespace PhysicsBlock3D
{

	/**
	 * @brief 基础三维网格类
	 * @details 由 2D 版 BaseGrid 改写。核心变化：
	 *  1. 新增 depth 维度，索引布局从 x*height+y 变为 x*height*depth + y*depth + z
	 *     （x 步长 height*depth，y 步长 depth，z 步长 1）。
	 *  2. 线段碰撞检测同时提供两套算法，由 `PhysicsBlock_LineTrace3D` 切换：
	 *     - 1 = Amanatides-Woo 三维 DDA（默认，直接给出入射面）；
	 *     - 0 = 三维 Bresenham（整数步进，与 2D 同构）。
	 *     两套实现都在本类内（`TraceLineDDA` / `TraceLineBresenham`），
	 *     `DDADetection` 作为统一入口按宏分派，调用点无需改动。
	 *  3. 方向编码从 2 bit（4 向）扩到 3 bit（6 面）。
	 */
	class BaseGrid3D SerializationInherit3D_
	{
#if PhysicsBlock_Serialization3D
	public:
		/**
		 * @brief 序列化虚函数
		 */
		SerializationVirtualFunction3D;

		/**
		 * @brief 从 JSON 数据构造基础三维网格
		 * @param data JSON 数据
		 */
		BaseGrid3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
		    : width(data["width"]), height(data["height"]), depth(data["depth"])
		{
			NewBool = data["NewBool"];
			if (NewBool)
			{
				Grid = new GridBlock3D[static_cast<size_t>(width) * height * depth];
			}
			JsonContrarySerialization(data);
		}
#endif
	public:
		/**
		 * @brief 网格宽度（X 轴格数）
		 */
		const unsigned int width;

		/**
		 * @brief 网格高度（Y 轴格数）
		 */
		const unsigned int height;

		/**
		 * @brief 网格深度（Z 轴格数）
		 */
		const unsigned int depth;

		/**
		 * @brief 网格是否在内部生成
		 * @details true 表示由本类内部创建，析构时释放
		 */
		bool NewBool = false;

		/**
		 * @brief 网格数据指针
		 * @details 数组大小为 width * height * depth
		 */
		GridBlock3D *Grid;

		/**
		 * @brief 获取网格块（不安全）
		 * @warning 坐标越界会导致未定义行为
		 */
		virtual inline GridBlock3D &at(int x, int y, int z)
		{
			return at(static_cast<unsigned int>((x * static_cast<int>(height) + y) * static_cast<int>(depth) + z));
		}

		/**
		 * @brief 获取网格块（不安全）
		 */
		virtual inline GridBlock3D &at(IVec3_ pos)
		{
			return at(pos.x, pos.y, pos.z);
		}

		/**
		 * @brief 获取网格块（不安全，按线性索引）
		 */
		virtual inline GridBlock3D &at(unsigned int i) { return Grid[i]; }

		/**
		 * @brief 只读获取网格块（const 版本，供 const 成员函数内部查询使用）
		 * @note 2D 版没有 const 访问路径，导致 const 成员函数无法读取网格；
		 *       3D 补上，避免为绕过 const 而把查询函数写成非 const。
		 */
		inline const GridBlock3D &GridAt(int x, int y, int z) const
		{
			return Grid[(x * static_cast<int>(height) + y) * static_cast<int>(depth) + z];
		}

		/**
		 * @brief 只读获取网格块（const 版本）
		 */
		inline const GridBlock3D &GridAt(IVec3_ pos) const
		{
			return GridAt(pos.x, pos.y, pos.z);
		}

		/**
		 * @brief 坐标是否在网格内
		 */
		inline bool InBounds(int x, int y, int z) const
		{
			return x >= 0 && y >= 0 && z >= 0 &&
			       x < static_cast<int>(width) &&
			       y < static_cast<int>(height) &&
			       z < static_cast<int>(depth);
		}

		/**
		 * @brief 坐标是否在网格内
		 */
		inline bool InBounds(IVec3_ pos) const { return InBounds(pos.x, pos.y, pos.z); }

		/**
		 * @brief 获取是否有碰撞（安全）
		 * @return 有障碍物返回 true；范围外返回 false
		 */
		virtual inline bool GetCollision(unsigned int x, unsigned int y, unsigned int z)
		{
			if (x >= width || y >= height || z >= depth)
			{
				return false;
			}
			return at(x, y, z).Collision;
		}

		/**
		 * @brief 获取是否有碰撞（安全）
		 */
		virtual inline bool GetCollision(UVec3_ Pos)
		{
			return GetCollision(Pos.x, Pos.y, Pos.z);
		}

	public:
		/**
		 * @brief 构造函数（内部创建网格）
		 */
		BaseGrid3D(const unsigned int Width, const unsigned int Height, const unsigned int Depth);

		/**
		 * @brief 构造函数（外部提供网格）
		 * @warning 外部网格需使用者自行管理生命周期
		 */
		BaseGrid3D(const unsigned int Width, const unsigned int Height, const unsigned int Depth,
		           GridBlock3D *Gridptr);

		/**
		 * @brief 析构函数
		 */
		~BaseGrid3D();

		/**
		 * @brief 线段碰撞检测（整数体素坐标）
		 * @param start 起始位置（体素坐标）
		 * @param end 结束位置（体素坐标）
		 * @return 碰撞结果信息
		 * @details 按 `PhysicsBlock_LineTrace3D` 分派到 `TraceLineDDA` 或
		 *          `TraceLineBresenham`，调用方无需关心当前用的是哪套算法。
		 */
		CollisionInfoI3D DDADetection(IVec3_ start, IVec3_ end);

		/**
		 * @brief 线段碰撞检测（浮点世界坐标）
		 * @param start 起始位置（世界坐标）
		 * @param end 结束位置（世界坐标）
		 * @return 碰撞结果信息（含精确碰撞位置、入射面方向、摩擦）
		 * @details 同上，按宏分派。
		 */
		CollisionInfoD3D DDADetection(Vec3_ start, Vec3_ end);

		// ═══════════════════════════════════════════════════════════════════
		// 两套算法本体：可显式调用（不受宏影响），便于对比、测试或按需选用
		// ═══════════════════════════════════════════════════════════════════

		/**
		 * @brief Amanatides-Woo 三维 DDA（整数体素坐标入口）
		 */
		CollisionInfoI3D TraceLineDDA(IVec3_ start, IVec3_ end);

		/**
		 * @brief Amanatides-Woo 三维 DDA（浮点世界坐标入口）
		 * @details 参数 t 比较推进；推进时记录穿过的轴与方向，
		 *          精确入射点 = start + dir * tEnter，入射面由该轴直接得出。
		 */
		CollisionInfoD3D TraceLineDDA(Vec3_ start, Vec3_ end);

		/**
		 * @brief 三维 Bresenham（整数体素坐标入口）
		 * @details 纯整数误差累积，无浮点累积误差、无乘除法（与 2D 版同构）。
		 *          只给出"经过哪些体素"，不含精确入射点。
		 */
		CollisionInfoI3D TraceLineBresenham(IVec3_ start, IVec3_ end);

		/**
		 * @brief 三维 Bresenham（浮点世界坐标入口）
		 * @details 用整数 Bresenham 找出命中的体素，再用射线与该体素 AABB
		 *          求交（`RayVoxelEntry`）反推精确入射点与入射面。
		 */
		CollisionInfoD3D TraceLineBresenham(Vec3_ start, Vec3_ end);

	private:
		/**
		 * @brief 求射线进入某个体素立方体的参数 t 与入射面
		 * @param o 射线起点（3 分量）
		 * @param d 射线方向（未归一化，3 分量；t 为该参数下的距离）
		 * @param voxel 目标体素（其立方体为 [voxel, voxel+1)）
		 * @param tEnter 输出：进入该体素的参数 t
		 * @param axis 输出：进入时穿过的轴（0=X,1=Y,2=Z；-1 表示起点已在内部）
		 * @param step 输出：该轴上的前进方向（+1/-1）
		 * @return 射线是否与该体素相交
		 * @details slab 法（三轴分别求进出参数取交叠区间）。
		 *          供 Bresenham 反推精确碰撞点用——Bresenham 只给体素序列，
		 *          精确位置必须再几何求一次，这是两套算法的本质差异。
		 */
		static bool RayVoxelEntry(const FLOAT_ o[3], const FLOAT_ d[3], IVec3_ voxel,
		                          FLOAT_ &tEnter, int &axis, int &step);

	public:

		/**
		 * @brief 由轴向与步进方向得到入射面编码
		 * @param axis 0=X, 1=Y, 2=Z
		 * @param step 步进方向（正/负）
		 * @return CheckDirection 编码（0..5）
		 * @details 沿 +X 前进时是从 -X 面进入 → Right(0)；沿 -X 前进从 +X 面进入 → Left(2)。
		 *          同理 Y 轴 → Down(3)/Up(1)，Z 轴 → Forward(4)/Backward(5)。
		 */
		static inline unsigned char FaceFromStep(int axis, int step)
		{
			static const unsigned char table[3][2] = {
			    {2, 0}, // X: step<0 → +X(Left=2)，step>0 → -X(Right=0)
			    {1, 3}, // Y: step<0 → +Y(Up=1)，  step>0 → -Y(Down=3)
			    {5, 4}, // Z: step<0 → +Z(Backward=5)，step>0 → -Z(Forward=4)
			};
			return table[axis][step > 0 ? 1 : 0];
		}
	};

}
