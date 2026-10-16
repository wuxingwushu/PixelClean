#pragma once
#include "BaseStruct3D.hpp"
#include "BaseSerialization3D.hpp"
#include "BaseCalculate3D.hpp"

namespace PhysicsBlock3D
{

	/**
	 * @brief 基础三维网格类
	 * @details 由 2D 版 BaseGrid 改写。核心变化：
	 *  1. 新增 depth 维度，索引布局从 x*height+y 变为 x*height*depth + y*depth + z
	 *     （x 步长 height*depth，y 步长 depth，z 步长 1）。
	 *  2. Bresenham 线段检测替换为 Amanatides-Woo 三维 DDA：
	 *     Bresenham 只有 2 个 tie-break 分支，扩展到 3 轴后歧义组合爆炸且
	 *     无法给出精确入射面；DDA 用参数 t 比较天然支持任意轴数，
	 *     且直接给出穿过的是哪个面（6 面法线），供 Direction 使用。
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
		 * @details 三维 DDA 遍历线段经过的体素，遇到 Collision 即返回。
		 */
		CollisionInfoI3D DDADetection(IVec3_ start, IVec3_ end);

		/**
		 * @brief 线段碰撞检测（浮点世界坐标）
		 * @param start 起始位置（世界坐标）
		 * @param end 结束位置（世界坐标）
		 * @return 碰撞结果信息（含精确碰撞位置、入射面方向、摩擦）
		 */
		CollisionInfoD3D DDADetection(Vec3_ start, Vec3_ end);

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
