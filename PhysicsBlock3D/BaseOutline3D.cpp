#include "BaseOutline3D.hpp"
#include <algorithm>

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

	BaseOutline3D::BaseOutline3D(const unsigned int Width, const unsigned int Height, const unsigned int Depth)
	    : BaseGrid3D(Width, Height, Depth)
	{
		// 表面点数上界约 6*W*H（最坏情况薄片），预留以减少重分配
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
	 * @brief 取种子点
	 * @details 从表面体素沿法线反向深入，找到该方向上的最深层实体体素中心。
	 *          2D 用"最大贪婪矩形中心"（O(N²) 枚举），3D 改用单向深入（O(深度)）。
	 *          两者都满足"在形状内部、且从它出发能射到该表面点"的用途。
	 */
	Vec3_ BaseOutline3D::FindSeedPoint(IVec3_ surfaceVoxel, IVec3_ normal) const
	{
		IVec3_ cur = surfaceVoxel;
		IVec3_ deepest = surfaceVoxel;
		// 沿 -normal 方向深入，最多走到网格边界
		const int maxDepth = static_cast<int>(width + height + depth);
		for (int i = 0; i < maxDepth; ++i)
		{
			const IVec3_ next(cur.x - normal.x, cur.y - normal.y, cur.z - normal.z);
			if (!IsSolid(next.x, next.y, next.z))
			{
				break;
			}
			cur = next;
			deepest = next;
		}
		return Vec3_(static_cast<FLOAT_>(deepest.x), static_cast<FLOAT_>(deepest.y),
		             static_cast<FLOAT_>(deepest.z)) +
		       Vec3_(0.5f, 0.5f, 0.5f);
	}

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

		if (mode == 0)
		{
			// ── 完整版：枚举所有实体体素的 6 个暴露面
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
						const FLOAT_ fric = at(x, y, z).FrictionFactor;
						for (int f = 0; f < 6; ++f)
						{
							const Vec3_ &n = kFaceNormals[f];
							const int nx = x + static_cast<int>(n.x);
							const int ny = y + static_cast<int>(n.y);
							const int nz = z + static_cast<int>(n.z);
							if (IsSolid(nx, ny, nz))
							{
								continue; // 邻居也是实体 → 该面不暴露
							}
							// 面中心 = 体素中心 + 0.5 * 法线
							const Vec3_ faceCentre =
							    Vec3_(static_cast<FLOAT_>(x) + 0.5f, static_cast<FLOAT_>(y) + 0.5f,
							          static_cast<FLOAT_>(z) + 0.5f) +
							    n * 0.5f;
							const Vec3_ seed = FindSeedPoint(IVec3_(x, y, z), IVec3_(static_cast<int>(n.x),
							                                                          static_cast<int>(n.y),
							                                                          static_cast<int>(n.z)));
							OutlineSet.emplace_back(faceCentre, n, fric, seed, IVec3_(x, y, z));
						}
					}
				}
			}
		}
		else if (mode == 1)
		{
			// ── 轻量版：逐方向逐切片做行程合并（run-length），
			//    点数从 O(面积) 降到 O(各切片周长之和)
			for (int f = 0; f < 6; ++f)
			{
				const Vec3_ &n = kFaceNormals[f];
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
							IVec3_ voxel(0, 0, 0);
							if (v < dimV)
							{
								int c[3];
								c[axisN] = s;
								c[axisU] = u;
								c[axisV] = v;
								voxel = IVec3_(c[0], c[1], c[2]);
								if (IsSolid(c[0], c[1], c[2]) &&
								    !IsSolid(c[0] + static_cast<int>(n.x),
								             c[1] + static_cast<int>(n.y),
								             c[2] + static_cast<int>(n.z)))
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
								// 输出 [runStart, v-1] 的中点
								const int mid = (runStart + v - 1) / 2;
								int c[3];
								c[axisN] = s;
								c[axisU] = u;
								c[axisV] = mid;
								const IVec3_ midVoxel(c[0], c[1], c[2]);

								// 沿法线方向的实际面中心
								const Vec3_ faceCentre =
								    Vec3_(static_cast<FLOAT_>(c[0]) + 0.5f, static_cast<FLOAT_>(c[1]) + 0.5f,
								          static_cast<FLOAT_>(c[2]) + 0.5f) +
								    n * 0.5f;
								const FLOAT_ fric = at(c[0], c[1], c[2]).FrictionFactor;
								const Vec3_ seed = FindSeedPoint(midVoxel, IVec3_(static_cast<int>(n.x),
								                                                   static_cast<int>(n.y),
								                                                   static_cast<int>(n.z)));
								OutlineSet.emplace_back(faceCentre, n, fric, seed, midVoxel);
								runStart = -1;
							}
						}
					}
				}
			}
		}
		else
		{
			// ── 最小版：每个方向只保留切平面上的 4 个极值点（支撑点）
			for (int f = 0; f < 6; ++f)
			{
				const Vec3_ &n = kFaceNormals[f];
				const int axisU = kFaceAxes[f][1];
				const int axisV = kFaceAxes[f][2];

				bool found = false;
				Vec3_ best[4]; // 对应 (u+v), (u-v), (-u+v), (-u-v) 四个极值
				IVec3_ bestVoxel[4];
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
							if (IsSolid(x + static_cast<int>(n.x), y + static_cast<int>(n.y),
							            z + static_cast<int>(n.z)))
							{
								continue;
							}
							const FLOAT_ pos[3] = {static_cast<FLOAT_>(x), static_cast<FLOAT_>(y),
							                       static_cast<FLOAT_>(z)};
							const FLOAT_ uu = pos[axisU];
							const FLOAT_ vv = pos[axisV];
							const FLOAT_ score[4] = {uu + vv, uu - vv, -uu + vv, -uu - vv};
							for (int k = 0; k < 4; ++k)
							{
								if (score[k] > bestScore[k])
								{
									bestScore[k] = score[k];
									best[k] = Vec3_(pos[0] + 0.5f, pos[1] + 0.5f, pos[2] + 0.5f) + n * 0.5f;
									bestVoxel[k] = IVec3_(x, y, z);
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
					const Vec3_ seed = FindSeedPoint(bestVoxel[k], IVec3_(static_cast<int>(n.x),
					                                                       static_cast<int>(n.y),
					                                                       static_cast<int>(n.z)));
					OutlineSet.emplace_back(best[k], n, fric, seed, bestVoxel[k]);
				}
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
				OutlineSet.push_back(p);
			}
		}
		OutlineSize = static_cast<unsigned int>(OutlineSet.size());
		CollisionR = data.contains("CollisionR") ? (FLOAT_)data["CollisionR"] : 0.0f;
	}
#endif

}
