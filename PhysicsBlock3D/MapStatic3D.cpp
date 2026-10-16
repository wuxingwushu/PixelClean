#include "MapStatic3D.hpp"
#include "BaseCalculate3D.hpp"
#include <algorithm>

namespace PhysicsBlock3D
{

	// ═══════════════════════════════════════════════════════════════
	//  轮廓提取的公共实现
	// ═══════════════════════════════════════════════════════════════

	namespace
	{
		/**
		 * @brief 6 个面方向（与 CheckDirection 一致）
		 * @details Right=0(-X), Up=1(+Y), Left=2(+X), Down=3(-Y),
		 *          Forward=4(-Z), Backward=5(+Z)
		 */
		const IVec3_ kFaceOffset3D[6] = {
		    IVec3_(-1, 0, 0), // Right    (-X)
		    IVec3_(0, 1, 0),  // Up       (+Y)
		    IVec3_(1, 0, 0),  // Left     (+X)
		    IVec3_(0, -1, 0), // Down     (-Y)
		    IVec3_(0, 0, -1), // Forward  (-Z)
		    IVec3_(0, 0, 1),  // Backward (+Z)
		};

		/**
		 * @brief 给定面法线轴，返回该面内的 4 个相邻方向
		 * @param faceIdx 面索引 0..5
		 * @param out 输出 4 个面内方向（垂直于面法线），前两个为轴 A、后两个为轴 B
		 * @note 轴 A / 轴 B 的划分用于判定「凸角」：只有当两条面内轴
		 *       各自至少有一个邻居为空时，该面片才位于凸角上。
		 */
		inline void InPlaneOffsets(int faceIdx, IVec3_ out[4])
		{
			// 面法线所在轴
			const int axis = (faceIdx == 0 || faceIdx == 2) ? 0
			                 : (faceIdx == 1 || faceIdx == 3) ? 1
			                                                  : 2;
			int n = 0;
			for (int a = 0; a < 3; ++a)
			{
				if (a == axis)
					continue;
				for (int s = -1; s <= 1; s += 2)
				{
					IVec3_ d(0);
					// 逐轴赋值（IVec3_ 支持 operator[]）
					d[a] = s;
					out[n++] = d;
				}
			}
		}

	}

	/**
	 * @brief 设置地图质心（越界忽略）
	 */
	void MapStatic3D::SetCentrality(Vec3_ Centrality)
	{
		if (Centrality.x < 0 || Centrality.y < 0 || Centrality.z < 0 ||
		    Centrality.x > (FLOAT_)width || Centrality.y > (FLOAT_)height ||
		    Centrality.z > (FLOAT_)depth)
		{
			return;
		}
		centrality = Centrality;
	}

	/**
	 * @brief 遍历区域内的实体体素，按 mode 提取暴露面
	 * @param mode 0=完整（所有暴露面）1=轻量（凸边上的面）2=最小（凸角上的面）
	 * @details 2D 版按「角点」提取（每个实体格检查 4 个角）；
	 *          3D 版按「面」提取（每个实体体素检查 6 个面），
	 *          面中心 = 体素中心 + 0.5·外向法线，
	 *          与窄相 `d.pos - d.face*0.5` 回到体素中心的约定一致。
		 */
	static void ExtractFaces(BaseGrid3D *grid, Vec3_ centrality,
	                         int x_, int y_, int z_, int w_, int h_, int d_,
	                         int mode, std::vector<MapOutline3D> &Outline)
	{
		// 世界坐标 → 体素坐标（加质心）
		x_ += (int)centrality.x;
		y_ += (int)centrality.y;
		z_ += (int)centrality.z;
		w_ += (int)centrality.x;
		h_ += (int)centrality.y;
		d_ += (int)centrality.z;

		const int W = (int)grid->width;
		const int H = (int)grid->height;
		const int D = (int)grid->depth;

		auto solid = [&](int x, int y, int z) -> bool {
			if (x < 0 || y < 0 || z < 0 || x >= W || y >= H || z >= D)
				return false; // 界外视为空
			return grid->GetCollision((unsigned int)x, (unsigned int)y, (unsigned int)z);
		};

		for (int x = x_; x < w_; ++x)
		{
			for (int y = y_; y < h_; ++y)
			{
				for (int z = z_; z < d_; ++z)
				{
					if (!solid(x, y, z))
						continue;

					const FLOAT_ friction = grid->at(x, y, z).FrictionFactor;

					for (int f = 0; f < 6; ++f)
					{
						const IVec3_ &off = kFaceOffset3D[f];
						// 邻居是实体 → 该面不暴露
						if (solid(x + off.x, y + off.y, z + off.z))
							continue;

						if (mode != 0)
						{
							IVec3_ inPlane[4];
							InPlaneOffsets(f, inPlane);

							// 四个面内邻居的空/实
							const bool empty[4] = {
							    !solid(x + inPlane[0].x, y + inPlane[0].y, z + inPlane[0].z),
							    !solid(x + inPlane[1].x, y + inPlane[1].y, z + inPlane[1].z),
							    !solid(x + inPlane[2].x, y + inPlane[2].y, z + inPlane[2].z),
							    !solid(x + inPlane[3].x, y + inPlane[3].y, z + inPlane[3].z),
							};

							if (mode == 1)
							{
								// 轻量：至少一个面内邻居为空（面片边界上的面）
								if (!empty[0] && !empty[1] && !empty[2] && !empty[3])
									continue;
							}
							else
							{
								// 最小：每条面内轴上都至少有一个邻居为空
								// （即该面位于凸棱/凸角上）
								// inPlane[0]/[1] 同属第一条面内轴，
								// inPlane[2]/[3] 同属第二条面内轴
								if ((!empty[0] && !empty[1]) ||
								    (!empty[2] && !empty[3]))
									continue;
							}
						}

						// 面中心 = 体素中心 + 0.5 · 外向法线
						const Vec3_ normal = Vec3_(off);
						const Vec3_ center = Vec3_((FLOAT_)x + 0.5f, (FLOAT_)y + 0.5f,
						                           (FLOAT_)z + 0.5f);
						Outline.push_back(MapOutline3D(center + normal * 0.5f, normal, friction));
					}
				}
			}
		}
	}

	void MapStatic3D::GetLightweightOutline(int x_, int y_, int z_, int w_, int h_, int d_,
	                                        std::vector<MapOutline3D> &Outline)
	{
		ExtractFaces(this, centrality, x_, y_, z_, w_, h_, d_, 1, Outline);
	}

	void MapStatic3D::GetOutline(int x_, int y_, int z_, int w_, int h_, int d_,
	                             std::vector<MapOutline3D> &Outline)
	{
		ExtractFaces(this, centrality, x_, y_, z_, w_, h_, d_, 0, Outline);
	}

	void MapStatic3D::GetMinOutline(int x_, int y_, int z_, int w_, int h_, int d_,
	                                std::vector<MapOutline3D> &Outline)
	{
		ExtractFaces(this, centrality, x_, y_, z_, w_, h_, d_, 2, Outline);
	}

	/**
	 * @brief 三维 DDA（世界坐标）
	 * @details 世界坐标加质心 → 裁剪到地图盒内 → DDA → 命中点减回质心。
	 */
	CollisionInfoD3D MapStatic3D::FMDDADetection(Vec3_ start, Vec3_ end)
	{
		start += centrality;
		end += centrality;

		const BoxFocus focus = LineBoxFocus(start, end, Vec3_((FLOAT_)width, (FLOAT_)height,
		                                                      (FLOAT_)depth));
		if (focus.Focus)
		{
			CollisionInfoD3D info = DDADetection(focus.start, focus.end);
			if (info.Collision)
			{
				info.pos -= centrality;
				return info;
			}
		}
		CollisionInfoD3D miss;
		miss.Collision = false;
		return miss;
	}

	/**
	 * @brief 带边界保护的三维 DDA（体素坐标）
	 */
	CollisionInfoI3D MapStatic3D::FMSafeDDADetection(IVec3_ start, IVec3_ end)
	{
		const int W = (int)width - 1;
		const int H = (int)height - 1;
		const int D = (int)depth - 1;
		start.x = std::max(0, std::min(start.x, W));
		start.y = std::max(0, std::min(start.y, H));
		start.z = std::max(0, std::min(start.z, D));
		end.x = std::max(0, std::min(end.x, W));
		end.y = std::max(0, std::min(end.y, H));
		end.z = std::max(0, std::min(end.z, D));
		return DDADetection(start, end);
	}

	/**
	 * @brief 安全设置体素碰撞标志
	 */
	bool MapStatic3D::SafeSetCollision(IVec3_ pos, bool state)
	{
		if (pos.x < 0 || pos.y < 0 || pos.z < 0 ||
		    pos.x >= (int)width || pos.y >= (int)height || pos.z >= (int)depth)
		{
			return false;
		}
		GridBlock3D &block = at(pos.x, pos.y, pos.z);
		if (block.Collision != state)
		{
			block.Collision = state;
			if (!state)
			{
				block.Collision = false;
				block.Healthpoint = 0;
			}
			NotifyCollisionChange(pos, state);
			return true;
		}
		return false;
	}

	void MapStatic3D::SetFriction(int x, int y, int z, FLOAT_ friction)
	{
		if (x < 0 || y < 0 || z < 0 ||
		    x >= (int)width || y >= (int)height || z >= (int)depth)
		{
			return;
		}
		at(x, y, z).FrictionFactor = friction;
	}

	FLOAT_ MapStatic3D::GetFriction(int x, int y, int z)
	{
		if (x < 0 || y < 0 || z < 0 ||
		    x >= (int)width || y >= (int)height || z >= (int)depth)
		{
			return FLOAT_{};
		}
		return at(x, y, z).FrictionFactor;
	}

}
