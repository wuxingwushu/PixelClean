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
	 * @brief 线段碰撞检测（整数体素坐标）—— 统一入口
	 * @details 按 `PhysicsBlock_LineTrace3D` 分派：
	 *          1 = DDA（转发到浮点重载，用体素中心构造射线）；
	 *          0 = 三维 Bresenham（纯整数步进）。
	 *          两套算法都只给"第一个 Collision 的体素"，对外语义一致，
	 *          因此调用点（PsDDADetection / FMDDADetection ...）无需改动。
	 */
	CollisionInfoI3D BaseGrid3D::DDADetection(IVec3_ start, IVec3_ end)
	{
#if PhysicsBlock_LineTrace3D
		return TraceLineDDA(start, end);
#else
		return TraceLineBresenham(start, end);
#endif
	}

	/**
	 * @brief 线段碰撞检测（浮点世界坐标）—— 统一入口
	 */
	CollisionInfoD3D BaseGrid3D::DDADetection(Vec3_ start, Vec3_ end)
	{
#if PhysicsBlock_LineTrace3D
		return TraceLineDDA(start, end);
#else
		return TraceLineBresenham(start, end);
#endif
	}

	// ═══════════════════════════════════════════════════════════════════════
	// 算法一：Amanatides-Woo 三维 DDA
	// ═══════════════════════════════════════════════════════════════════════

	/**
	 * @brief 线段碰撞检测（整数体素坐标）
	 * @details 直接用整数体素中心构造射线，转发到浮点重载。
	 *          2D 版此处是独立的 Bresenham 循环，3D 版统一到一条遍历路径，
	 *          避免两份遍历代码行为不一致（2D 的 BaseGrid 与 MapDynamic
	 *          各有一份 Bresenham，判据的等号处理还不一样）。
	 */
	CollisionInfoI3D BaseGrid3D::TraceLineDDA(IVec3_ start, IVec3_ end)
	{
		CollisionInfoI3D result;
		result.Collision = false;
		result.pos = IVec3_(0, 0, 0);
		result.Friction = FLOAT_{};

		// 用体素中心作为端点，保证射线确实穿过这些体素
		const CollisionInfoD3D info = TraceLineDDA(Vec3_(start) + Vec3_(0.5f),
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
	CollisionInfoD3D BaseGrid3D::TraceLineDDA(Vec3_ start, Vec3_ end)
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

			// 参数越过线段终点（t>1）即已走出线段，不能再把下一格当作命中：
			// 终点格不一定按"最小 tMax"的顺序到达（斜线时最后一步可能先切别的轴），
			// 不在这里截断就会把线段之外的体素也遍历进来，命中点跑到终点之外。
			if (tMax[axis] > 1.0f)
			{
				break;
			}

			tEnter = tMax[axis];
			pos[axis] += step[axis];
			tMax[axis] += tDelta[axis];
			lastAxis = axis;
			lastStep = step[axis];
		}

		return result;
	}

	// ═══════════════════════════════════════════════════════════════════════
	// 算法二：三维 Bresenham（整数步进，与 2D 版同构）
	// ═══════════════════════════════════════════════════════════════════════

	/**
	 * @brief 三维 Bresenham 线段遍历（整数体素坐标）
	 * @details 2D 版只有 err 一个误差项；三维需要"主轴 + 两个误差项"：
	 *          1. 主轴 = 三个分量里跨度最大的轴（d[mainAxis] 最大）；
	 *             主轴决定循环步数，另两轴为副轴 a、b。
	 *          2. 副轴各带一个误差项（整数、移位/加法实现，无浮点）：
	 *               errA = 2*d[a] - d[main]
	 *               errB = 2*d[b] - d[main]
	 *             每步若 errA > 0 则副轴 a 进一格并 errA -= 2*d[main]，
	 *             随后 errA += 2*d[a]；副轴 b 同理。
	 *          3. 循环体：先判命中，再调副轴，最后主轴进一格。
	 *
	 *          与 DDA 的本质差异：Bresenham 产生的是"数字直线"（每步副轴
	 *          至多进一格、整数误差累积），DDA 产生的是"几何直线"经过的格子
	 *          （参数 t 比较）。两者在斜线、对角线附近可能差一格，
	 *          这正是保留两套算法做对比的意义。
	 * @note 全程整数运算：无浮点累积误差、无乘除法（仅初始化时算 2*d）。
	 *       主轴自适应（而非固定 X）保证线段沿 Y/Z 方向时不会跳格。
	 */
	CollisionInfoI3D BaseGrid3D::TraceLineBresenham(IVec3_ start, IVec3_ end)
	{
		CollisionInfoI3D result;
		result.Collision = false;
		result.pos = IVec3_(0, 0, 0);
		result.Friction = FLOAT_{};

		IVec3_ p = start;

		int d[3] = {std::abs(end.x - start.x),
		            std::abs(end.y - start.y),
		            std::abs(end.z - start.z)};
		int s[3] = {(start.x < end.x) ? 1 : ((start.x > end.x) ? -1 : 0),
		            (start.y < end.y) ? 1 : ((start.y > end.y) ? -1 : 0),
		            (start.z < end.z) ? 1 : ((start.z > end.z) ? -1 : 0)};

		// ── 主轴 = 跨度最大的轴 ──
		int mainAxis = 0;
		if (d[1] > d[mainAxis]) mainAxis = 1;
		if (d[2] > d[mainAxis]) mainAxis = 2;
		const int aAxis = (mainAxis + 1) % 3;
		const int bAxis = (mainAxis + 2) % 3;

		const int dm = d[mainAxis];
		const int twoDm = dm << 1;
		const int twoDa = d[aAxis] << 1;
		const int twoDb = d[bAxis] << 1;

		int errA = twoDa - dm; // 副轴 a 的误差项
		int errB = twoDb - dm; // 副轴 b 的误差项

		// 步数上界：三轴跨度之和 + 余量，防止退化输入导致死循环
		const int maxSteps = d[0] + d[1] + d[2] + 3;

		for (int i = 0; i <= maxSteps; ++i)
		{
			// 命中判定（与 DDA 同语义：越界不算命中）
			if (InBounds(p))
			{
				GridBlock3D &block = at(p.x, p.y, p.z);
				if (block.Collision)
				{
					result.Collision = true;
					result.pos = p;
					result.Friction = block.FrictionFactor;
					return result;
				}
			}

			if (p.x == end.x && p.y == end.y && p.z == end.z)
			{
				break;
			}
			if (dm == 0)
			{
				break; // 起点与终点同格（三轴跨度均为 0）
			}

			// ── 副轴按误差项决定是否进一格 ──
			if (errA > 0)
			{
				p[aAxis] += s[aAxis];
				errA -= twoDm;
			}
			errA += twoDa;

			if (errB > 0)
			{
				p[bAxis] += s[bAxis];
				errB -= twoDm;
			}
			errB += twoDb;

			// ── 主轴必进一格 ──
			p[mainAxis] += s[mainAxis];
		}

		return result;
	}

	/**
	 * @brief 三维 Bresenham（浮点世界坐标）
	 * @details Bresenham 只给"经过哪些体素"，不含精确入射点/入射面，
	 *          所以命中后再用 `RayVoxelEntry`（slab 法）对该体素求一次几何交，
	 *          把 pos 与 Direction 补全，使其与 DDA 版本的返回值语义一致
	 *          （调用点只认 CollisionInfoD3D 的字段，不关心算法）。
	 */
	CollisionInfoD3D BaseGrid3D::TraceLineBresenham(Vec3_ start, Vec3_ end)
	{
		CollisionInfoD3D result;
		result.Collision = false;
		result.Friction = FLOAT_{};
		result.pos = start;
		result.GridPos = IVec3_(0, 0, 0);
		result.Direction = 0;

		// 体素序列用整数遍历；两端直接用 ToInt（floor），
		// 与 DDA 的初始体素约定保持一致
		const CollisionInfoI3D info = TraceLineBresenham(ToInt(start), ToInt(end));
		if (!info.Collision)
		{
			return result;
		}

		result.Collision = true;
		result.GridPos = info.pos;
		result.Friction = info.Friction;

		// 命中体素已定，几何反推精确入射点与入射面
		const Vec3_ dir = end - start;
		const FLOAT_ o[3] = {start.x, start.y, start.z};
		const FLOAT_ d[3] = {dir.x, dir.y, dir.z};

		FLOAT_ tEnter = 0.0f;
		int axis = -1;
		int step = 0;
		if (RayVoxelEntry(o, d, info.pos, tEnter, axis, step))
		{
			result.pos = start + dir * tEnter;
			if (axis >= 0)
			{
				result.Direction = FaceFromStep(axis, step);
			}
			else
			{
				// 起点就在障碍内：取运动反方向的入射面（与 DDA 分支一致）
				int dom = 0;
				if (std::abs(d[1]) > std::abs(d[dom])) dom = 1;
				if (std::abs(d[2]) > std::abs(d[dom])) dom = 2;
				result.Direction = FaceFromStep(dom, (d[dom] > 0.0f) ? -1 : 1);
			}
		}
		else
		{
			// 理论上不该发生（命中的体素必然被射线穿过）；
			// 退化输入（start==end 等）时退回到体素中心，保证返回值可用
			result.pos = Vec3_(info.pos) + Vec3_(0.5f);
		}

		return result;
	}

	/**
	 * @brief 求射线进入某个体素立方体的参数 t 与入射面（slab 法）
	 * @details 体素 voxel 占据 [voxel, voxel+1) 立方体。三轴分别求射线与该轴
	 *          两块平行板（voxel[i] 与 voxel[i]+1）的交点参数，取三者
	 *          "进/出区间"的交叠；交叠区间的下界即进入该体素的 tEnter，
	 *          贡献下界的那个轴就是入射轴。
	 *          d[i]==0 时该轴不限制（射线平行于板），除非起点已在板外。
	 */
	bool BaseGrid3D::RayVoxelEntry(const FLOAT_ o[3], const FLOAT_ d[3], IVec3_ voxel,
	                               FLOAT_ &tEnter, int &axis, int &step)
	{
		tEnter = 0.0f;
		axis = -1;
		step = 0;

		FLOAT_ tMin = 0.0f; // 进入参数（射线从 t=0 出发，故下界不小于 0）
		FLOAT_ tMax = 1.0f; // 退出参数（线段参数域是 [0,1]）
		int enterAxis = -1;
		int enterStep = 0;

		for (int i = 0; i < 3; ++i)
		{
			const FLOAT_ lo = (FLOAT_)voxel[i];
			const FLOAT_ hi = lo + 1.0f;

			if (d[i] == 0.0f)
			{
				// 平行于该轴的板：起点必须落在板内
				if (o[i] < lo || o[i] > hi)
				{
					return false;
				}
				continue;
			}

			const FLOAT_ inv = 1.0f / d[i];
			FLOAT_ t0 = (lo - o[i]) * inv;
			FLOAT_ t1 = (hi - o[i]) * inv;
			int s = (d[i] > 0.0f) ? 1 : -1;
			if (t0 > t1)
			{
				const FLOAT_ tmp = t0;
				t0 = t1;
				t1 = tmp;
			}

			if (t0 > tMin)
			{
				tMin = t0;
				enterAxis = i;
				enterStep = s;
			}
			if (t1 < tMax)
			{
				tMax = t1;
			}
			if (tMin > tMax)
			{
				return false;
			}
		}

		tEnter = tMin;
		axis = enterAxis;
		step = enterStep;
		return true;
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
