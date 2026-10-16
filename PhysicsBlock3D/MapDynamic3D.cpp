#include "MapDynamic3D.hpp"
#include <algorithm>
#include <cassert>

namespace PhysicsBlock3D
{

	namespace
	{
		/** @brief 六个轴向面偏移（与 CheckDirection 的 Right/Up/Left/Down/Forward/Backward 一致） */
		const IVec3_ kFaceOffset3D[6] = {
		    IVec3_(-1, 0, 0), IVec3_(0, 1, 0), IVec3_(1, 0, 0),
		    IVec3_(0, -1, 0), IVec3_(0, 0, -1), IVec3_(0, 0, 1)};

		/**
		 * @brief 面内四个邻居偏移
		 * @details 前两个同属第一条面内轴，后两个同属第二条面内轴。
		 */
		inline void InPlaneOffsets(int faceIdx, IVec3_ out[4])
		{
			switch (faceIdx)
			{
			case 0: // -X 面：面内为 Y / Z
			case 2: // +X 面
				out[0] = IVec3_(0, -1, 0);
				out[1] = IVec3_(0, 1, 0);
				out[2] = IVec3_(0, 0, -1);
				out[3] = IVec3_(0, 0, 1);
				break;
			case 1: // +Y 面：面内为 X / Z
			case 3: // -Y 面
				out[0] = IVec3_(-1, 0, 0);
				out[1] = IVec3_(1, 0, 0);
				out[2] = IVec3_(0, 0, -1);
				out[3] = IVec3_(0, 0, 1);
				break;
			default: // ±Z 面：面内为 X / Y
				out[0] = IVec3_(-1, 0, 0);
				out[1] = IVec3_(1, 0, 0);
				out[2] = IVec3_(0, -1, 0);
				out[3] = IVec3_(0, 1, 0);
				break;
			}
		}

		/**
		 * @brief 提取区域的暴露面
		 * @param grid 目标网格（其 `at()` 为虚函数，会自动派发到 MapDynamic3D 的板块查找）
		 * @param mode 0=全部暴露面，1=轻量，2=最小（只留凸角/棱）
		 * @details 与 `MapStatic3D.cpp` 的 `ExtractFaces` 同构，差别是本函数直接使用
		 *          地图本地体素坐标（动态地图 centrality 恒为 0），不做偏移。
		 */
		void ExtractFacesDynamic(BaseGrid3D *grid, int x_, int y_, int z_, int w_, int h_, int d_,
		                         int mode, std::vector<MapOutline3D> &Outline)
		{
			auto solid = [grid](int x, int y, int z) -> bool {
				if (x < 0 || y < 0 || z < 0)
					return false;
				if (x >= static_cast<int>(grid->width) || y >= static_cast<int>(grid->height) ||
				    z >= static_cast<int>(grid->depth))
					return false;
				return grid->at(x, y, z).Collision;
			};

			for (int x = x_; x < w_; ++x)
			{
				for (int y = y_; y < h_; ++y)
				{
					for (int z = z_; z < d_; ++z)
					{
						if (!solid(x, y, z))
							continue;

						for (int face = 0; face < 6; ++face)
						{
							const IVec3_ &n = kFaceOffset3D[face];
							if (solid(x + n.x, y + n.y, z + n.z))
								continue;

							if (mode > 0)
							{
								IVec3_ inPlane[4];
								InPlaneOffsets(face, inPlane);

								bool empty[4];
								for (int i = 0; i < 4; ++i)
								{
									empty[i] = !solid(x + n.x + inPlane[i].x, y + n.y + inPlane[i].y,
									                  z + n.z + inPlane[i].z);
								}

								if (mode == 1)
								{
									// 轻量：至少一个面内邻居为空
									if (!empty[0] && !empty[1] && !empty[2] && !empty[3])
										continue;
								}
								else
								{
									// 最小：每条面内轴上都至少一个邻居为空
									if ((!empty[0] && !empty[1]) || (!empty[2] && !empty[3]))
										continue;
								}
							}

							const Vec3_ center = Vec3_(x + 0.5f, y + 0.5f, z + 0.5f);
							const Vec3_ normal = Vec3_(static_cast<FLOAT_>(n.x), static_cast<FLOAT_>(n.y),
							                           static_cast<FLOAT_>(n.z));
							Outline.push_back(MapOutline3D(center + normal * 0.5f, normal,
							                               grid->at(x, y, z).FrictionFactor));
						}
					}
				}
			}
		}

		/**
		 * @brief 获取最大 2 的幂次（floor(log2(val))）
		 */
		int PlatePower(unsigned int val, int i = -1)
		{
			return val > 0 ? PlatePower(val >>= 1, ++i) : i;
		}
	}

	/**
	 * @brief 构造函数 - 创建动态三维地图
	 * @details 由 2D `MapDynamic` 的贪心**方块**分配推广为贪心**立方体**分配：
	 *          对每个尚未分配的板块，向三个方向同时扩张求最大的空闲立方体，
	 *          取不超过它的最大 2 的幂，再按 Morton3D 顺序填充该立方体，
	 *          使相邻板块在内存中也尽量相邻（提高缓存命中率）。
	 */
	MapDynamic3D::MapDynamic3D(const unsigned int Width, const unsigned int Height,
	                           const unsigned int Depth)
	    : width(Width), height(Height), depth(Depth),
	      mMovePlate(Width, Height, Depth, PixelBlockEdgeSize3D, Width / 2, Height / 2, Depth / 2),
	      centrality(Vec3_(static_cast<FLOAT_>(Width / 2 * PixelBlockEdgeSize3D),
	                       static_cast<FLOAT_>(Height / 2 * PixelBlockEdgeSize3D),
	                       static_cast<FLOAT_>(Depth / 2 * PixelBlockEdgeSizeZ3D))),
	      BaseGrid3D(Width * PixelBlockEdgeSize3D, Height * PixelBlockEdgeSize3D,
	                 Depth * PixelBlockEdgeSizeZ3D, nullptr)
	{
		type = PhysicsObjectEnum::_MapDynamic;

		const size_t plateCount = static_cast<size_t>(width) * height * depth;
		const size_t voxelPerPlate =
		    static_cast<size_t>(PixelBlockEdgeSize3D) * PixelBlockEdgeSize3D * PixelBlockEdgeSizeZ3D;

		BaseGridBuffer = static_cast<BaseGrid3D *>(::operator new(plateCount * sizeof(BaseGrid3D)));
		GridBuffer = new GridBlock3D[plateCount * voxelPerPlate];

		for (size_t i = 0; i < plateCount; ++i)
		{
			new (&BaseGridBuffer[i]) BaseGrid3D(PixelBlockEdgeSize3D, PixelBlockEdgeSize3D,
			                                    PixelBlockEdgeSizeZ3D, &GridBuffer[voxelPerPlate * i]);
		}

		bool *DataBool = new bool[plateCount];
		for (size_t i = 0; i < plateCount; ++i)
		{
			DataBool[i] = false;
		}

		auto Index = [this](unsigned int x, unsigned int y, unsigned int z) -> size_t {
			return (static_cast<size_t>(x) * height + y) * depth + z;
		};

		/* 将网格分配到移动板块控制中心，让相邻的坐标内存也尽可能的相邻（提高内存命中率） */

		unsigned int GetNum = 0;
		unsigned int minDim = width < height ? width : height;
		if (depth < minDim)
		{
			minDim = depth;
		}

		for (unsigned int x = 0; x < width; ++x)
		{
			for (unsigned int y = 0; y < height; ++y)
			{
				for (unsigned int z = 0; z < depth; ++z)
				{
					if (DataBool[Index(x, y, z)])
						continue;

					// 向三个方向同时扩张，求最大的空闲立方体边长 e
					unsigned int e;
					for (e = 1; e < minDim; ++e)
					{
						const unsigned int fx = x + e;
						const unsigned int fy = y + e;
						const unsigned int fz = z + e;
						if (fx >= width || fy >= height || fz >= depth)
						{
							break;
						}

						bool OK = false;
						for (unsigned int xx = x; xx <= fx && !OK; ++xx)
						{
							for (unsigned int yy = y; yy <= fy && !OK; ++yy)
							{
								for (unsigned int zz = z; zz <= fz; ++zz)
								{
									if (DataBool[Index(xx, yy, zz)])
									{
										OK = true;
										break;
									}
								}
							}
						}
						if (OK)
						{
							break;
						}
					}

					if (e < 2)
					{
						*mMovePlate.PlateAt(x, y, z) = &BaseGridBuffer[GetNum];
						++GetNum;
						continue;
					}

					int i = PlatePower(e);
					i = static_cast<int>(1) << i;
					const unsigned int PY = GetNum;
					for (int posx = 0; posx < i; ++posx)
					{
						for (int posy = 0; posy < i; ++posy)
						{
							for (int posz = 0; posz < i; ++posz)
							{
								++GetNum;
								const unsigned int MortonVal = static_cast<unsigned int>(
								    PY + Morton3D(static_cast<uint_fast32_t>(posx),
								                  static_cast<uint_fast32_t>(posy),
								                  static_cast<uint_fast32_t>(posz)));
								assert((MortonVal < plateCount) &&
								       "BaseDefine3D 的 PixelBlockEdgeSize3D 只可以是 2 的幂次方");
								assert(!DataBool[Index(x + posx, y + posy, z + posz)] &&
								       "BaseDefine3D 的 PixelBlockEdgeSize3D 只可以是 2 的幂次方");
								DataBool[Index(x + posx, y + posy, z + posz)] = true;
								*mMovePlate.PlateAt(x + posx, y + posy, z + posz) =
								    &BaseGridBuffer[MortonVal];
							}
						}
					}
				}
			}
		}
		delete[] DataBool;
	}

	/**
	 * @brief 析构函数 - 释放所有板块网格资源
	 * @note 与 2D 的 `delete (char*)BaseGridBuffer` / `delete GridBuffer`（与 new[] 不匹配）
	 *       不同，3D 用 `::operator delete` 与 `::operator new` 配对，`delete[]` 与 `new[]` 配对。
	 */
	MapDynamic3D::~MapDynamic3D()
	{
		const size_t plateCount = static_cast<size_t>(width) * height * depth;
		for (size_t i = 0; i < plateCount; ++i)
		{
			BaseGridBuffer[i].~BaseGrid3D();
		}
		::operator delete(BaseGridBuffer);
		delete[] GridBuffer;
	}

	/**
	 * @brief 安全三维 DDA（边界裁剪）
	 */
	CollisionInfoI3D MapDynamic3D::FMSafeDDADetection(IVec3_ start, IVec3_ end)
	{
		auto ClampAxis = [](int v, unsigned int n) -> int {
			if (v < 0)
				return 0;
			if (v >= static_cast<int>(n))
				return static_cast<int>(n) - 1;
			return v;
		};
		start.x = ClampAxis(start.x, BaseGrid3D::width);
		start.y = ClampAxis(start.y, BaseGrid3D::height);
		start.z = ClampAxis(start.z, BaseGrid3D::depth);
		end.x = ClampAxis(end.x, BaseGrid3D::width);
		end.y = ClampAxis(end.y, BaseGrid3D::height);
		end.z = ClampAxis(end.z, BaseGrid3D::depth);
		return FMDDADetection(start, end);
	}

	void MapDynamic3D::GetLightweightOutline(int x_, int y_, int z_, int w_, int h_, int d_,
	                                         std::vector<MapOutline3D> &Outline)
	{
		ExtractFacesDynamic(this, x_, y_, z_, w_, h_, d_, 1, Outline);
	}

	void MapDynamic3D::GetOutline(int x_, int y_, int z_, int w_, int h_, int d_,
	                              std::vector<MapOutline3D> &Outline)
	{
		ExtractFacesDynamic(this, x_, y_, z_, w_, h_, d_, 0, Outline);
	}

	void MapDynamic3D::GetMinOutline(int x_, int y_, int z_, int w_, int h_, int d_,
	                                 std::vector<MapOutline3D> &Outline)
	{
		ExtractFacesDynamic(this, x_, y_, z_, w_, h_, d_, 2, Outline);
	}

	/**
	 * @brief 安全设置碰撞状态（带越界检查和回调通知）
	 */
	bool MapDynamic3D::SafeSetCollision(IVec3_ pos, bool state)
	{
		if (pos.x < 0 || pos.y < 0 || pos.z < 0 ||
		    static_cast<unsigned int>(pos.x) >= BaseGrid3D::width ||
		    static_cast<unsigned int>(pos.y) >= BaseGrid3D::height ||
		    static_cast<unsigned int>(pos.z) >= BaseGrid3D::depth)
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
			// 通知碰撞状态变化（通过 MapFormwork3D 桥接器转发到 PhysicsCollision3D）
			NotifyCollisionChange(pos, state);
			return true;
		}
		return false;
	}

	/**
	 * @brief 设置指定位置的摩擦力系数（带越界检查）
	 */
	void MapDynamic3D::SetFriction(int x, int y, int z, FLOAT_ friction)
	{
		if (x < 0 || y < 0 || z < 0 ||
		    static_cast<unsigned int>(x) >= BaseGrid3D::width ||
		    static_cast<unsigned int>(y) >= BaseGrid3D::height ||
		    static_cast<unsigned int>(z) >= BaseGrid3D::depth)
		{
			return;
		}
		at(x, y, z).FrictionFactor = friction;
	}

	/**
	 * @brief 获取指定位置的摩擦力系数（带越界检查）
	 */
	FLOAT_ MapDynamic3D::GetFriction(int x, int y, int z)
	{
		if (x < 0 || y < 0 || z < 0 ||
		    static_cast<unsigned int>(x) >= BaseGrid3D::width ||
		    static_cast<unsigned int>(y) >= BaseGrid3D::height ||
		    static_cast<unsigned int>(z) >= BaseGrid3D::depth)
		{
			return FLOAT_{};
		}
		return at(x, y, z).FrictionFactor;
	}

}
