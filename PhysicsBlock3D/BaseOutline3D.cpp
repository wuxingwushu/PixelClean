#include "BaseOutline3D.hpp"
#include <algorithm>
#include <map>
#include <tuple>
#include <vector>

namespace PhysicsBlock3D
{

	/** @brief 6 个轴向面法线（与 CheckDirection 顺序一致） */
	static const Vec3_ kFaceNormals[6] = {
	    Vec3_(-1.0f, 0.0f, 0.0f), // 0 Right
	    Vec3_(0.0f, 1.0f, 0.0f),  // 1 Up
	    Vec3_(1.0f, 0.0f, 0.0f),  // 2 Left
	    Vec3_(0.0f, -1.0f, 0.0f), // 3 Down
	    Vec3_(0.0f, 0.0f, -1.0f), // 4 Forward
	    Vec3_(0.0f, 0.0f, 1.0f),  // 5 Backward
	};

	/** @brief 每个方向对应的（法线轴, 切向轴1, 切向轴2） */
	static const int kFaceAxes[6][3] = {
	    {0, 1, 2}, {1, 0, 2}, {0, 1, 2}, {1, 0, 2}, {2, 0, 1}, {2, 0, 1},
	};

	/**
	 * @brief 方形面 4 个顶点在面内参数 (U,V) 中的取法
	 * @details 下标即 OutlinePoint3D::Corner，顺序为沿面环绕（相邻两个
	 *          顶点在面上共享一条边），因此 0-1-2-3 是一个正确闭环。
	 */
	static const int kCornerUV[4][2] = {
	    {0, 0}, {1, 0}, {1, 1}, {0, 1},
	};

	/**
	 * @brief 需要交换 (u,v) 的面
	 * @details kCornerUV 的绕序由 "U×V 与外向法线同向" 决定：同向时按
	 *          kCornerUV 走就是从面外侧看逆时针；反向的面（0/1/4）交换 u/v
	 *          恰好翻转绕序，使 4 个顶点仍然构成朝外的多边形。
	 *          逐面核对 kFaceAxes 即得：0/1/4 反向，2/3/5 同向。
	 */
	static const bool kCornerSwapUV[6] = {true, true, false, false, true, false};

	/**
	 * @brief 取方形面第 corner 个顶点在两条切向轴上的偏移量
	 * @param face 面编号 0..5
	 * @param corner 顶点序号 0..3
	 * @param cu 输出：沿 axisU 的偏移（0 或 1）
	 * @param cv 输出：沿 axisV 的偏移（0 或 1）
	 * @details 返回的是"物理"切向轴的偏移（kFaceAxes 的 axisU / axisV），
	 *          已处理交换，调用方直接用 voxel[axisU]+cu、voxel[axisV]+cv。
	 */
	static inline void CornerUV(int face, int corner, int &cu, int &cv)
	{
		if (kCornerSwapUV[face])
		{
			cu = kCornerUV[corner][1];
			cv = kCornerUV[corner][0];
		}
		else
		{
			cu = kCornerUV[corner][0];
			cv = kCornerUV[corner][1];
		}
	}

	/**
	 * @brief 反查顶点序号
	 * @param face 面编号
	 * @param cu 沿 axisU 的偏移
	 * @param cv 沿 axisV 的偏移
	 * @return 该偏移对应的 Corner 序号 0..3（找不到时回退 0）
	 */
	static inline int CornerIndex(int face, int cu, int cv)
	{
		for (int k = 0; k < 4; ++k)
		{
			int u0, v0;
			CornerUV(face, k, u0, v0);
			if (u0 == cu && v0 == cv)
			{
				return k;
			}
		}
		return 0;
	}

	/**
	 * @brief 由面内参数构造顶点的网格坐标
	 * @param voxel 面所在体素
	 * @param face 面编号 0..5
	 * @param u 切向 U 轴上的网格坐标
	 * @param v 切向 V 轴上的网格坐标
	 * @details 法线轴上取该侧的整格边界：外向法线为正取 voxel+1，为负取 voxel。
	 *          结果落在局部网格的整数点上。
	 */
	static Vec3_ FaceVertex(const IVec3_ &voxel, int face, int u, int v)
	{
		const Vec3_ &n = kFaceNormals[face];
		const int axisN = kFaceAxes[face][0];
		const int axisU = kFaceAxes[face][1];
		const int axisV = kFaceAxes[face][2];

		int c[3] = {voxel.x, voxel.y, voxel.z};
		c[axisN] += (n[axisN] > 0.0f) ? 1 : 0;
		c[axisU] = u;
		c[axisV] = v;
		return Vec3_(static_cast<FLOAT_>(c[0]), static_cast<FLOAT_>(c[1]),
		             static_cast<FLOAT_>(c[2]));
	}

	BaseOutline3D::BaseOutline3D(const unsigned int Width, const unsigned int Height, const unsigned int Depth)
	    : BaseGrid3D(Width, Height, Depth)
	{
		// 预留容量：运行期 UpdateAll 走最小版（固定 24 点），2*W*H 已绰绰有余；
		// 完整版（4 点/暴露面，薄片最坏约 8*W*H 点）若被调用则由 vector 自行扩容，
		// 不在构造时就按最坏情况吃内存。
		const size_t guess = static_cast<size_t>(Width) * Height * 2 + 64;
		OutlineSet.reserve(guess);
		OutlineSize = 0;
		CollisionR = 0.0f;
	}

	BaseOutline3D::~BaseOutline3D()
	{
		// vector 自动释放
	}

	/**
	 * @brief 实心体素的三维前缀和
	 * @details 把"任意轴对齐长方体是否全是实体"变成 O(1) 查询，
	 *          是 3D 版 MaxOutlineCentre（最大内接长方体）的基础。
	 */
	struct SolidPrefix3D
	{
		int W, H, D;
		int PlaneH, PlaneD;
		std::vector<long long> Sum; // (W+1)*(H+1)*(D+1)

		SolidPrefix3D(const GridBlock3D *grid, int w, int h, int d)
		    : W(w), H(h), D(d), PlaneH(h + 1), PlaneD(d + 1),
		      Sum(static_cast<size_t>(w + 1) * (h + 1) * (d + 1), 0)
		{
			for (int i = 0; i < W; ++i)
			{
				for (int j = 0; j < H; ++j)
				{
					for (int k = 0; k < D; ++k)
					{
						const long long solid = grid[(i * H + j) * D + k].Entity ? 1 : 0;
						Sum[Idx(i + 1, j + 1, k + 1)] =
						    solid
						    + Sum[Idx(i, j + 1, k + 1)] + Sum[Idx(i + 1, j, k + 1)] + Sum[Idx(i + 1, j + 1, k)]
						    - Sum[Idx(i, j, k + 1)] - Sum[Idx(i, j + 1, k)] - Sum[Idx(i + 1, j, k)]
						    + Sum[Idx(i, j, k)];
					}
				}
			}
		}

		inline size_t Idx(int i, int j, int k) const
		{
			return static_cast<size_t>(i * PlaneH + j) * PlaneD + k;
		}

		/** @brief 闭区间 [lo, hi] 内是否全为实体体素 */
		inline bool IsFull(const int lo[3], const int hi[3]) const
		{
			const long long c = Sum[Idx(hi[0] + 1, hi[1] + 1, hi[2] + 1)]
			                    - Sum[Idx(lo[0], hi[1] + 1, hi[2] + 1)]
			                    - Sum[Idx(hi[0] + 1, lo[1], hi[2] + 1)]
			                    - Sum[Idx(hi[0] + 1, hi[1] + 1, lo[2])]
			                    + Sum[Idx(lo[0], lo[1], hi[2] + 1)]
			                    + Sum[Idx(lo[0], hi[1] + 1, lo[2])]
			                    + Sum[Idx(hi[0] + 1, lo[1], lo[2])]
			                    - Sum[Idx(lo[0], lo[1], lo[2])];
			const long long v = static_cast<long long>(hi[0] - lo[0] + 1) *
			                    (hi[1] - lo[1] + 1) * (hi[2] - lo[2] + 1);
			return c == v;
		}
	};

	/**
	 * @brief 「最大外骨骼点质心」场
	 * @details 对每个实体体素，求**包含它的最大内接轴对齐长方体**，取长方体中心 ——
	 *          这正是 2D `BaseOutline::MaxOutlineCentre` 的定义（最大贪婪网格的中间位置），
	 *          所以不依赖面法线：同一体素的所有暴露面共用同一个质心。
	 *
	 *          旧实现"沿 -法线单向深入取最深处体素中心"是 1D 思路，放到 3D 里不成立：
	 *          3×3×3 实心立方会算出 9 种不同的"质心"（每个面按自己的切向坐标深入），
	 *          而正确结果只有一个 —— 立方体中心 (1.5, 1.5, 1.5)。
	 *
	 *          长方体搜索：从体素自身出发，每轮在 6 个方向里挑"扩展后体积最大"的那一个，
	 *          扩展距离用二分（盒子嵌套 ⇒ 可行距离单调），直到任何方向都扩不动为止。
	 *          凸形状下得到的就是整个形状；不规则形状下得到的是一个极大内接长方体。
	 */
	class SeedField3D
	{
	public:
		SeedField3D(const GridBlock3D *grid, int w, int h, int d)
		    : W(w), H(h), D(d), Prefix(grid, w, h, d),
		      Cache(static_cast<size_t>(w) * h * d, -1)
		{
		}

		/** @brief 取体素 (x,y,z) 的最大外骨骼点质心（体素网格浮点坐标） */
		Vec3_ Of(int x, int y, int z)
		{
			const size_t idx = (static_cast<size_t>(x) * H + y) * D + z;
			if (Cache[idx] >= 0)
			{
				return Seed[static_cast<size_t>(Cache[idx])];
			}

			int lo[3] = {x, y, z};
			int hi[3] = {x, y, z};
			const int dim[3] = {W, H, D};
			for (;;)
			{
				int bestAxis = -1, bestSign = 0, bestDist = 0;
				long long bestVol = 0;
				for (int axis = 0; axis < 3; ++axis)
				{
					for (int sign = -1; sign <= 1; sign += 2)
					{
						const int limit = (sign < 0) ? lo[axis] : (dim[axis] - 1 - hi[axis]);
						if (limit <= 0)
						{
							continue;
						}
						// 二分：最大的还能保持整块实心的扩展距离
						int dist = 0, l = 1, r = limit;
						while (l <= r)
						{
							const int mid = (l + r) / 2;
							int tlo[3] = {lo[0], lo[1], lo[2]};
							int thi[3] = {hi[0], hi[1], hi[2]};
							(sign < 0 ? tlo[axis] : thi[axis]) = (sign < 0) ? lo[axis] - mid : hi[axis] + mid;
							if (Prefix.IsFull(tlo, thi))
							{
								dist = mid;
								l = mid + 1;
							}
							else
							{
								r = mid - 1;
							}
						}
						if (dist == 0)
						{
							continue;
						}
						const long long vol = static_cast<long long>(hi[0] - lo[0] + 1 +
						                                             (axis == 0 ? dist : 0)) *
						                      (hi[1] - lo[1] + 1 + (axis == 1 ? dist : 0)) *
						                      (hi[2] - lo[2] + 1 + (axis == 2 ? dist : 0));
						if (vol > bestVol)
						{
							bestVol = vol;
							bestAxis = axis;
							bestSign = sign;
							bestDist = dist;
						}
					}
				}
				if (bestAxis < 0)
				{
					break; // 六个方向都扩不动 → 已是极大内接长方体
				}
				if (bestSign < 0)
				{
					lo[bestAxis] -= bestDist;
				}
				else
				{
					hi[bestAxis] += bestDist;
				}
			}

			// 与 2D MaxOutlineCentre 一致：中心 = (min + max + 1) / 2
			const Vec3_ centre(
			    static_cast<FLOAT_>(lo[0] + hi[0] + 1) * 0.5f,
			    static_cast<FLOAT_>(lo[1] + hi[1] + 1) * 0.5f,
			    static_cast<FLOAT_>(lo[2] + hi[2] + 1) * 0.5f);
			Seed.push_back(centre);
			Cache[idx] = static_cast<int>(Seed.size()) - 1;
			return Seed.back();
		}

	private:
		int W, H, D;
		SolidPrefix3D Prefix;
		std::vector<int> Cache;  // 每体素 → Seed 下标（-1 未计算）
		std::vector<Vec3_> Seed; // 去重后的质心表（同一长方体的体素共享）
	};

	/**
	 * @brief 表面点收集
	 * @param mode 0=完整 1=轻量 2=最小
	 */
	void BaseOutline3D::BuildOutline(int mode, Vec3_ CentreMass_)
	{
		OutlineSet.clear();
		OutlineSize = 0;
		CollisionR = 0.0f;

		const int W = static_cast<int>(width);
		const int H = static_cast<int>(height);
		const int D = static_cast<int>(depth);

		// 每个实体体素所属「最大内外骨骼长方体」的中心（2D MaxOutlineCentre 的 3D 版），
		// 逐体素缓存：同一体素的 6 个面共用同一个质心
		SeedField3D seeds(Grid, W, H, D);

		if (mode == 0)
		{
			// ── 完整版：枚举所有实体体素的 6 个暴露面，每个面输出 4 个顶点
			//    顶点按 Corner 0..3 环绕，因此每连续 4 点就是一个方形面
			for (int x = 0; x < W; ++x)
			{
				for (int y = 0; y < H; ++y)
				{
					for (int z = 0; z < D; ++z)
					{
						if (!at(x, y, z).Entity)
						{
							continue;
						}
						const IVec3_ voxel(x, y, z);
						const FLOAT_ fric = at(x, y, z).FrictionFactor;
						for (int f = 0; f < 6; ++f)
						{
							const Vec3_ &n = kFaceNormals[f];
							const IVec3_ nv(static_cast<int>(n.x), static_cast<int>(n.y),
							                static_cast<int>(n.z));
							if (IsSolid(x + nv.x, y + nv.y, z + nv.z))
							{
								continue; // 邻居也是实体 → 该面不暴露
							}
							const int axisU = kFaceAxes[f][1];
							const int axisV = kFaceAxes[f][2];
							// 4 个顶点共享一个射线起点（从内部射向面顶点）
							const Vec3_ seed = seeds.Of(voxel.x, voxel.y, voxel.z);
							for (int k = 0; k < 4; ++k)
							{
								int cu, cv;
								CornerUV(f, k, cu, cv);
								const Vec3_ vertex =
								    FaceVertex(voxel, f, voxel[axisU] + cu, voxel[axisV] + cv);
								OutlineSet.emplace_back(vertex, n, fric, seed, voxel,
								                        static_cast<unsigned char>(k));
							}
						}
					}
				}
			}
		}
		else if (mode == 1)
		{
			// ── 轻量版：逐方向逐切片做行程合并（run-length），
			//    点数从 O(面积) 降到 O(各切片周长之和)。
			//    每个条带输出其矩形的 4 个角点（仍是方形面顶点，仍按 Corner 环绕）
			for (int f = 0; f < 6; ++f)
			{
				const Vec3_ &n = kFaceNormals[f];
				const IVec3_ nv(static_cast<int>(n.x), static_cast<int>(n.y),
				                static_cast<int>(n.z));
				const int axisN = kFaceAxes[f][0];
				const int axisU = kFaceAxes[f][1];
				const int axisV = kFaceAxes[f][2];

				const int dimN = (axisN == 0) ? W : (axisN == 1 ? H : D);
				const int dimU = (axisU == 0) ? W : (axisU == 1 ? H : D);
				const int dimV = (axisV == 0) ? W : (axisV == 1 ? H : D);

				for (int s = 0; s < dimN; ++s)
				{
					for (int u = 0; u < dimU; ++u)
					{
						int runStart = -1;
						for (int v = 0; v <= dimV; ++v)
						{
							// 判定该位置是否为暴露面
							bool exposed = false;
							if (v < dimV)
							{
								int c[3];
								c[axisN] = s;
								c[axisU] = u;
								c[axisV] = v;
								if (IsSolid(c[0], c[1], c[2]) &&
								    !IsSolid(c[0] + nv.x, c[1] + nv.y, c[2] + nv.z))
								{
									exposed = true;
								}
							}

							if (exposed && runStart < 0)
							{
								runStart = v;
							}
							else if (!exposed && runStart >= 0)
							{
								// 条带沿 v 覆盖 [runStart, v]，取其矩形的 4 个角点
								const int runEnd = v; // 开区间上界
								int c[3];
								c[axisN] = s;
								c[axisU] = u;
								c[axisV] = runStart;
								const FLOAT_ fric = at(c[0], c[1], c[2]).FrictionFactor;
								const Vec3_ seed = seeds.Of(c[0], c[1], c[2]);
								for (int k = 0; k < 4; ++k)
								{
									int cu, cv;
									CornerUV(f, k, cu, cv);
									// cv==0 → 条带下沿（runStart）；cv==1 → 上沿（runEnd），
									// 上沿角点属于最后一格面，故其体素取 runEnd-1
									int cc[3];
									cc[axisN] = s;
									cc[axisU] = u;
									cc[axisV] = (cv == 0) ? runStart : (runEnd - 1);
									const IVec3_ cornerVoxel(cc[0], cc[1], cc[2]);
									const Vec3_ vertex = FaceVertex(cornerVoxel, f, u + cu,
									                                (cv == 0) ? runStart : runEnd);
									OutlineSet.emplace_back(vertex, n, fric, seed, cornerVoxel,
									                        static_cast<unsigned char>(k));
								}
								runStart = -1;
							}
						}
					}
				}
			}
		}
		else
		{
			// ── 最小版：每个方向只保留切平面上的 4 个对角极值（支撑点），
			//    取极值面上的对应**顶点**（不再是面中心）
			static const int kDiagCorner[4][2] = {{1, 1}, {1, 0}, {0, 1}, {0, 0}};
			for (int f = 0; f < 6; ++f)
			{
				const Vec3_ &n = kFaceNormals[f];
				const IVec3_ nv(static_cast<int>(n.x), static_cast<int>(n.y),
				                static_cast<int>(n.z));
				const int axisU = kFaceAxes[f][1];
				const int axisV = kFaceAxes[f][2];

				bool found = false;
				Vec3_ best[4]; // 对应 (u+v), (u-v), (-u+v), (-u-v) 四个极值顶点
				IVec3_ bestVoxel[4];
				unsigned char bestCorner[4] = {0, 0, 0, 0};
				FLOAT_ bestScore[4];
				for (int k = 0; k < 4; ++k)
				{
					bestScore[k] = -FLOAT_MAX;
				}

				for (int x = 0; x < W; ++x)
				{
					for (int y = 0; y < H; ++y)
					{
						for (int z = 0; z < D; ++z)
						{
							if (!at(x, y, z).Entity)
							{
								continue;
							}
							if (IsSolid(x + nv.x, y + nv.y, z + nv.z))
							{
								continue;
							}
							int c[3] = {x, y, z};
							const FLOAT_ uu = static_cast<FLOAT_>(c[axisU]);
							const FLOAT_ vv = static_cast<FLOAT_>(c[axisV]);
							const FLOAT_ score[4] = {uu + vv, uu - vv, -uu + vv, -uu - vv};
							const IVec3_ voxel(x, y, z);
							for (int k = 0; k < 4; ++k)
							{
								if (score[k] > bestScore[k])
								{
									bestScore[k] = score[k];
									// 每个极值方向对应的角偏移是常量（各候选面相同），
									// 取顶点只是在 (u,v) 上统一 +0/+1，不改变极值排序
									best[k] = FaceVertex(voxel, f, c[axisU] + kDiagCorner[k][0],
									                     c[axisV] + kDiagCorner[k][1]);
									bestVoxel[k] = voxel;
									bestCorner[k] = static_cast<unsigned char>(
									    CornerIndex(f, kDiagCorner[k][0], kDiagCorner[k][1]));
									found = true;
								}
							}
						}
					}
				}

				if (!found)
				{
					continue;
				}
				for (int k = 0; k < 4; ++k)
				{
					if (bestScore[k] <= -FLOAT_MAX)
					{
						continue;
					}
					const FLOAT_ fric = at(bestVoxel[k].x, bestVoxel[k].y, bestVoxel[k].z).FrictionFactor;
					const Vec3_ seed = seeds.Of(bestVoxel[k].x, bestVoxel[k].y, bestVoxel[k].z);
					OutlineSet.emplace_back(best[k], n, fric, seed, bestVoxel[k], bestCorner[k]);
				}
			}
		}

		// ── 同一顶点只保留一条射线
		//    方形面的顶点会被相邻的 2~3 个面各输出一次。如果每个副本各自带一个
		//    "最大外骨骼点质心"，同一个可见的外骨骼点就会连出多条射线（可视化里
		//    正是从一点射向多个质心的交叉线），碰撞射线也会各走各的。
		//    这里按顶点局部坐标归并：同一顶点的所有副本统一改用离它最近的那个
		//    质心，于是"外骨骼点 → 质心"始终一对一，碰撞射线与画出来的连线一致。
		if (OutlineSet.size() > 1)
		{
			// 此时 LocalPos 还是整数网格坐标（还没减质心），可直接当键
			std::map<std::tuple<int, int, int>, size_t> vertexSlot;
			std::vector<Vec3_> seedOfVertex;
			seedOfVertex.reserve(OutlineSet.size());
			std::vector<size_t> slotOfPoint(OutlineSet.size());
			for (size_t i = 0; i < OutlineSet.size(); ++i)
			{
				const Vec3_ &pos = OutlineSet[i].LocalPos;
				const std::tuple<int, int, int> key(static_cast<int>(pos.x),
				                                    static_cast<int>(pos.y),
				                                    static_cast<int>(pos.z));
				const auto it = vertexSlot.find(key);
				if (it == vertexSlot.end())
				{
					slotOfPoint[i] = seedOfVertex.size();
					vertexSlot.emplace(key, seedOfVertex.size());
					seedOfVertex.push_back(OutlineSet[i].SeedPos);
				}
				else
				{
					slotOfPoint[i] = it->second;
					Vec3_ &best = seedOfVertex[it->second];
					// ModulusLength 返回平方长度，比较距离大小时无需开方
					if (ModulusLength(OutlineSet[i].SeedPos - pos) < ModulusLength(best - pos))
					{
						best = OutlineSet[i].SeedPos;
					}
				}
			}
			for (size_t i = 0; i < OutlineSet.size(); ++i)
			{
				OutlineSet[i].SeedPos = seedOfVertex[slotOfPoint[i]];
			}
		}

		// ── 平移到相对质心，并同步更新包围半径
		for (auto &p : OutlineSet)
		{
			p.LocalPos -= CentreMass_;
			p.SeedPos -= CentreMass_;
			const FLOAT_ r2 = ModulusLength(p.LocalPos);
			if (r2 > CollisionR)
			{
				CollisionR = r2;
			}
		}
		CollisionR = SQRT_(CollisionR);
		OutlineSize = static_cast<unsigned int>(OutlineSet.size());
	}

	void BaseOutline3D::UpdateOutline(Vec3_ CentreMass_)
	{
		BuildOutline(0, CentreMass_);
	}

	void BaseOutline3D::UpdateLightweightOutline(Vec3_ CentreMass_)
	{
		BuildOutline(1, CentreMass_);
	}

	void BaseOutline3D::UpdateMinOutline(Vec3_ CentreMass_)
	{
		BuildOutline(2, CentreMass_);
	}

	void BaseOutline3D::UpdateCollisionR()
	{
		FLOAT_ maxR2 = 0.0f;
		for (const auto &p : OutlineSet)
		{
			const FLOAT_ r2 = ModulusLength(p.LocalPos);
			if (r2 > maxR2)
			{
				maxR2 = r2;
			}
		}
		CollisionR = SQRT_(maxR2);
	}

#if PhysicsBlock_Serialization3D
	/**
	 * @brief 序列化轮廓点集合
	 * @note 3D 的轮廓是纯派生数据（可由网格重建），但仍序列化以支持
	 *       存档后立即碰撞而不必先调 UpdateAll()。
	 */
	void BaseOutline3D::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
	{
		BaseGrid3D::JsonSerialization(data);
		data["OutlineSize"] = OutlineSize;
		data["CollisionR"] = CollisionR;
		nlohmann::json_abi_v3_12_0::basic_json<> &arr = data["OutlineSet"];
		arr = arr.array();
		for (size_t i = 0; i < OutlineSet.size(); ++i)
		{
			// 注意：SerializationVec3 用 TOSIRING3D 把整个实参拼成键名，
			// 只对"裸标识符"（如 pos、Wind）成立；这里实参是 member 表达式，
			// 必须写字面键，否则读写两侧拼出的键不一致（序列化写
			// "OutlineSet[i].LocalPos.x"，反序列化读 "p.LocalPos.x"）。
			arr[i]["LocalPos"]["x"] = OutlineSet[i].LocalPos.x;
			arr[i]["LocalPos"]["y"] = OutlineSet[i].LocalPos.y;
			arr[i]["LocalPos"]["z"] = OutlineSet[i].LocalPos.z;
			arr[i]["Normal"]["x"] = OutlineSet[i].Normal.x;
			arr[i]["Normal"]["y"] = OutlineSet[i].Normal.y;
			arr[i]["Normal"]["z"] = OutlineSet[i].Normal.z;
			arr[i]["SeedPos"]["x"] = OutlineSet[i].SeedPos.x;
			arr[i]["SeedPos"]["y"] = OutlineSet[i].SeedPos.y;
			arr[i]["SeedPos"]["z"] = OutlineSet[i].SeedPos.z;
			arr[i]["Friction"] = OutlineSet[i].Friction;
			arr[i]["VoxelX"] = OutlineSet[i].Voxel.x;
			arr[i]["VoxelY"] = OutlineSet[i].Voxel.y;
			arr[i]["VoxelZ"] = OutlineSet[i].Voxel.z;
			arr[i]["Corner"] = static_cast<int>(OutlineSet[i].Corner);
		}
	}

	void BaseOutline3D::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
	{
		BaseGrid3D::JsonContrarySerialization(data);
		OutlineSet.clear();
		if (data.contains("OutlineSet"))
		{
			auto arr = data["OutlineSet"];
			OutlineSet.reserve(arr.size());
			for (size_t i = 0; i < arr.size(); ++i)
			{
				OutlinePoint3D p;
				p.LocalPos = Vec3_(arr[i]["LocalPos"]["x"].get<FLOAT_>(),
				                   arr[i]["LocalPos"]["y"].get<FLOAT_>(),
				                   arr[i]["LocalPos"]["z"].get<FLOAT_>());
				p.Normal = Vec3_(arr[i]["Normal"]["x"].get<FLOAT_>(),
				                 arr[i]["Normal"]["y"].get<FLOAT_>(),
				                 arr[i]["Normal"]["z"].get<FLOAT_>());
				p.SeedPos = Vec3_(arr[i]["SeedPos"]["x"].get<FLOAT_>(),
				                  arr[i]["SeedPos"]["y"].get<FLOAT_>(),
				                  arr[i]["SeedPos"]["z"].get<FLOAT_>());
				p.Friction = arr[i]["Friction"];
				p.Voxel = IVec3_(arr[i]["VoxelX"], arr[i]["VoxelY"], arr[i]["VoxelZ"]);
				// 旧存档没有 Corner 字段：缺省 0，等价于旧版"每面 1 点"
				p.Corner = arr[i].contains("Corner")
				               ? static_cast<unsigned char>(arr[i]["Corner"].get<int>())
				               : static_cast<unsigned char>(0);
				OutlineSet.push_back(p);
			}
		}
		OutlineSize = static_cast<unsigned int>(OutlineSet.size());
		CollisionR = data.contains("CollisionR") ? (FLOAT_)data["CollisionR"] : 0.0f;
	}
#endif

}
