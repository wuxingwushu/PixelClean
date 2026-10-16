#include "BaseGrid3D.hpp"

namespace PhysicsBlock3D
{

	/**
	 * @brief 构造函数（内部创建网格）
	 * @details 分配 width * height * depth 个 GridBlock3D
	 */
	BaseGrid3D::BaseGrid3D(const unsigned int Width, const unsigned int Height, const unsigned int Depth)
	    : width(Width), height(Height), depth(Depth)
	{
		Grid = new GridBlock3D[static_cast<size_t>(width) * height * depth];
		NewBool = true;
	}

	/**
	 * @brief 构造函数（外部提供网格）
	 * @warning 外部网格需使用者自行管理生命周期
	 */
	BaseGrid3D::BaseGrid3D(const unsigned int Width, const unsigned int Height, const unsigned int Depth,
	                       GridBlock3D *Gridptr)
	    : width(Width), height(Height), depth(Depth), Grid(Gridptr)
	{
		NewBool = false;
	}

	/**
	 * @brief 析构函数
	 */
	BaseGrid3D::~BaseGrid3D()
	{
		if (NewBool)
		{
			delete[] Grid;
		}
	}

	/**
	 * @brief 线段碰撞检测（整数体素坐标）
	 * @details 直接用整数体素中心构造射线，转发到浮点重载。
	 *          2D 版此处是独立的 Bresenham 循环，3D 版统一到 DDA 一条路径，
	 *          避免两份遍历代码行为不一致（2D 的 BaseGrid 与 MapDynamic
	 *          各有一份 Bresenham，判据的等号处理还不一样）。
	 */
	CollisionInfoI3D BaseGrid3D::DDADetection(IVec3_ start, IVec3_ end)
	{
		CollisionInfoI3D result;
		result.Collision = false;
		result.pos = IVec3_(0, 0, 0);
		result.Friction = FLOAT_{};

		// 用体素中心作为端点，保证射线确实穿过这些体素
		const CollisionInfoD3D info = DDADetection(Vec3_(start) + Vec3_(0.5f),
		                                           Vec3_(end) + Vec3_(0.5f));
		if (info.Collision)
		{
			result.Collision = true;
			result.pos = info.GridPos;
			result.Friction = info.Friction;
		}
		return result;
	}

	/**
	 * @brief 线段碰撞检测（浮点世界坐标）—— Amanatides-Woo 三维 DDA
	 * @details 相对 2D Bresenham 的改进：
	 *  1. 三轴统一处理，无 tie-break 分支组合爆炸；
	 *  2. 记录进入体素时穿过的轴与方向，直接得到入射面（6 面之一），
	 *     替代 2D 的 calcFromX/calcFromY 回切试探；
	 *  3. 精确碰撞点 = start + dir * tEnter，无需按格中心反推。
	 */
	CollisionInfoD3D BaseGrid3D::DDADetection(Vec3_ start, Vec3_ end)
	{
		CollisionInfoD3D result;
		result.Collision = false;
		result.Friction = FLOAT_{};
		result.pos = start;
		result.GridPos = IVec3_(0, 0, 0);
		result.Direction = 0;

		const Vec3_ dir = end - start;

		IVec3_ pos = ToInt(start);
		const IVec3_ endVoxel = ToInt(end);

		// ── 逐轴初始化步进方向、tMax（到下一个边界的参数距离）、tDelta（每格参数增量）
		int step[3];
		FLOAT_ tMax[3];
		FLOAT_ tDelta[3];

		const FLOAT_ o[3] = {start.x, start.y, start.z};
		const FLOAT_ d[3] = {dir.x, dir.y, dir.z};

		for (int i = 0; i < 3; ++i)
		{
			if (d[i] == 0.0f)
			{
				step[i] = 0;
				tMax[i] = FLOAT_MAX;
				tDelta[i] = FLOAT_MAX;
				continue;
			}
			step[i] = (d[i] > 0.0f) ? 1 : -1;
			tDelta[i] = 1.0f / std::abs(d[i]);

			// 下一个边界坐标：正向取 floor(o)+1，负向取 floor(o)
			const FLOAT_ boundary = (step[i] > 0) ? (FLOAT_)(ToInt(o[i]) + 1) : (FLOAT_)ToInt(o[i]);
			tMax[i] = (boundary - o[i]) / d[i];
			if (tMax[i] < 0.0f)
			{
				tMax[i] = 0.0f;
			}
		}

		// ── 遍历
		FLOAT_ tEnter = 0.0f;
		int lastAxis = -1;
		int lastStep = 0;

		// 体素数量上界，防止浮点异常导致死循环
		const size_t maxSteps =
		    static_cast<size_t>(std::abs(endVoxel.x - pos.x) + std::abs(endVoxel.y - pos.y) +
		                        std::abs(endVoxel.z - pos.z)) + 3;

		for (size_t iter = 0; iter <= maxSteps; ++iter)
		{
			// 命中判定
			if (InBounds(pos))
			{
				GridBlock3D &block = at(pos.x, pos.y, pos.z);
				if (block.Collision)
				{
					result.Collision = true;
					result.GridPos = pos;
					result.Friction = block.FrictionFactor;

					// 精确碰撞点
					result.pos = start + dir * tEnter;

					if (lastAxis >= 0)
					{
						// 进入本格时穿过的是 lastAxis 轴上的面
						result.Direction = FaceFromStep(lastAxis, lastStep);
					}
					else
					{
						// 起点就在障碍内：取运动反方向的入射面
						int dom = 0;
						if (std::abs(d[1]) > std::abs(d[dom])) dom = 1;
						if (std::abs(d[2]) > std::abs(d[dom])) dom = 2;
						result.Direction = FaceFromStep(dom, -step[dom]);
					}
					return result;
				}
			}

			// 到达终点格
			if (pos.x == endVoxel.x && pos.y == endVoxel.y && pos.z == endVoxel.z)
			{
				break;
			}

			// 推进到下一个体素：选 tMax 最小的轴
			int axis = 0;
			if (tMax[1] < tMax[axis]) axis = 1;
			if (tMax[2] < tMax[axis]) axis = 2;

			if (step[axis] == 0)
			{
				break; // 三个轴都无法推进（起点==终点或方向退化）
			}

			tEnter = tMax[axis];
			pos[axis] += step[axis];
			tMax[axis] += tDelta[axis];
			lastAxis = axis;
			lastStep = step[axis];
		}

		return result;
	}

#if PhysicsBlock_Serialization3D
	/**
	 * @brief 将网格状态序列化为 JSON
	 */
	void BaseGrid3D::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
	{
		data["width"] = width;
		data["height"] = height;
		data["depth"] = depth;
		data["NewBool"] = NewBool;
		nlohmann::json_abi_v3_12_0::basic_json<> &dataYY = data["Grid"];
		dataYY = dataYY.array();
		const size_t total = static_cast<size_t>(width) * height * depth;
		for (size_t i = 0; i < total; ++i)
		{
			dataYY[i]["type"] = Grid[i].type;
			dataYY[i]["FrictionFactor"] = Grid[i].FrictionFactor;
			dataYY[i]["Healthpoint"] = Grid[i].Healthpoint;
			dataYY[i]["mass"] = Grid[i].mass;
		}
	}

	/**
	 * @brief 从 JSON 数据反序列化网格状态
	 */
	void BaseGrid3D::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
	{
		NewBool = data["NewBool"];
		auto dataBF = data["Grid"];
		const size_t total = static_cast<size_t>(width) * height * depth;
		for (size_t i = 0; i < total; ++i)
		{
			Grid[i].type = dataBF[i]["type"];
			Grid[i].FrictionFactor = dataBF[i]["FrictionFactor"];
			Grid[i].Healthpoint = dataBF[i]["Healthpoint"];
			Grid[i].mass = dataBF[i]["mass"];
		}
	}
#endif

}
