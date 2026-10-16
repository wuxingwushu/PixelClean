#pragma once
#include <assert.h>
#include <type_traits>
#include <cstdlib>

namespace PhysicsBlock3D
{

	/**
	 * @brief 板块移动信息结构体（三维）
	 * @details 由 2D `MovePlateInfo` 扩展，新增 Z 方向移动量。
	 */
	struct MovePlateInfo3D
	{
		bool UpData = false; ///< 是否发生了更新
		int X = 0;           ///< X 方向的移动量
		int Y = 0;           ///< Y 方向的移动量
		int Z = 0;           ///< Z 方向的移动量
	};

	/**
	 * @brief 可移动板块管理器（三维模板类）
	 * @tparam PlateT 板块类型（需可平凡复制）
	 * @details 由 2D `MovePlate` 改写。核心变化：
	 *  1. 新增 Z 轴：板块数组索引从 `x*mNumberY+y` 变为
	 *     `(x*mNumberY+y)*mNumberZ+z`。
	 *  2. 新增 `MovePlateZ`，并且三维滚动有**两级跳过**：
	 *     - Z 方向滚动时，跳过 X 区域**或** Y 区域内的板块
	 *       （它们会被后续的 Y / X 滚动重新生成）；
	 *     - Y 方向滚动时，跳过 X 区域内的板块（沿用 2D 语义）。
	 *     滚动顺序固定为 **Z → Y → X**，跳过集合逐级收窄，
	 *     保证每个板块恰好被生成一次、且坐标为最终正确值。
	 *  3. 修正 2D 的两处内存缺陷：`new char[...]` 配 `delete`（应为
	 *     `delete[]`）、以及 `constexpr` 构造函数中不初始化内存。
	 *     3D 改为 `::operator new` / `::operator delete` 配对，
	 *     并在构造时对板块数组做值初始化。
	 */
	template <typename PlateT>
	class MovePlate3D
	{
		static_assert(std::is_trivially_copyable<PlateT>::value,
		              "MovePlate3D requires a trivially copyable PlateT type. "
		              "Use a pointer type or POD struct.");

		/**
		 * @brief 板块生成回调函数类型
		 * @param mT 板块指针
		 * @param x 板块的 X 坐标（世界板块坐标）
		 * @param y 板块的 Y 坐标
		 * @param z 板块的 Z 坐标
		 * @param Data 用户自定义数据
		 */
		typedef void (*_GenerateCallback)(PlateT *mT, int x, int y, int z, void *Data);

		/**
		 * @brief 板块销毁回调函数类型
		 */
		typedef void (*_DeleteCallback)(PlateT *mT, void *Data);

	private:
		_GenerateCallback mGenerateCallback = nullptr; ///< 生成回调函数指针
		void *mGenerateData = nullptr;                 ///< 生成回调的用户数据
		_DeleteCallback mDeleteCallback = nullptr;     ///< 销毁回调函数指针
		void *mDeleteData = nullptr;                   ///< 销毁回调的用户数据

		const unsigned int mEdge;                             ///< 单个板块的边长（体素）
		const unsigned int mNumberX, mNumberY, mNumberZ;      ///< 三个方向的板块数量
		const unsigned int mOriginX, mOriginY, mOriginZ;      ///< 中心板块的索引
		PlateT *mPlate = nullptr;                             ///< 板块数组（三维展开为一维）

		int mPosX = 0, mPosY = 0, mPosZ = 0; ///< 当前监测位置（板块坐标）

	private:
		/**
		 * @brief 临时缓冲区，用于板块移动时的数据暂存
		 * @details 大小取三个方向板块数的最大值。
		 */
		PlateT *LPlate = nullptr;

	public:
		[[nodiscard]] inline int GetPosX() { return mPosX; }
		[[nodiscard]] inline int GetPosY() { return mPosY; }
		[[nodiscard]] inline int GetPosZ() { return mPosZ; }

		/**
		 * @brief 获取当前中心板块的世界 X 坐标（体素）
		 */
		[[nodiscard]] inline int GetPlateX() const { return (mOriginX - mPosX) * mEdge; }
		[[nodiscard]] inline int GetPlateY() const { return (mOriginY - mPosY) * mEdge; }
		[[nodiscard]] inline int GetPlateZ() const { return (mOriginZ - mPosZ) * mEdge; }

		/**
		 * @brief 将世界坐标转换为板块数组索引
		 */
		[[nodiscard]] inline int GetCalculatePosX(int x) { return (x / (int)mEdge) - mPosX + mOriginX; }
		[[nodiscard]] inline int GetCalculatePosY(int y) { return (y / (int)mEdge) - mPosY + mOriginY; }
		[[nodiscard]] inline int GetCalculatePosZ(int z) { return (z / (int)mEdge) - mPosZ + mOriginZ; }

		/**
		 * @brief 构造函数
		 * @param NumberX X 方向的板块数量
		 * @param NumberY Y 方向的板块数量
		 * @param NumberZ Z 方向的板块数量
		 * @param Edge 单个板块的边长（体素）
		 * @param OriginX 中心板块的 X 索引
		 * @param OriginY 中心板块的 Y 索引
		 * @param OriginZ 中心板块的 Z 索引
		 */
		MovePlate3D(const unsigned int NumberX, const unsigned int NumberY, const unsigned int NumberZ,
		            const unsigned int Edge,
		            const unsigned int OriginX = 0, const unsigned int OriginY = 0,
		            const unsigned int OriginZ = 0)
		    : mNumberX(NumberX), mNumberY(NumberY), mNumberZ(NumberZ), mEdge(Edge),
		      mOriginX(OriginX), mOriginY(OriginY), mOriginZ(OriginZ)
		{
			assert(mEdge != 0 && "Error : mEdge = 0");
			assert(mNumberX > 0 && mNumberY > 0 && mNumberZ > 0 && "Error : plate count = 0");

			const size_t count = (size_t)mNumberX * mNumberY * mNumberZ;
			// 用 ::operator new 分配裸内存并按值初始化（PlateT 需可平凡复制）
			mPlate = static_cast<PlateT *>(::operator new(count * sizeof(PlateT)));
			for (size_t i = 0; i < count; ++i)
			{
				new (&mPlate[i]) PlateT();
			}

			const unsigned int maxDim = mNumberX > mNumberY
			                                ? (mNumberX > mNumberZ ? mNumberX : mNumberZ)
			                                : (mNumberY > mNumberZ ? mNumberY : mNumberZ);
			LPlate = static_cast<PlateT *>(::operator new((size_t)maxDim * sizeof(PlateT)));
			for (unsigned int i = 0; i < maxDim; ++i)
			{
				new (&LPlate[i]) PlateT();
			}
		}

		~MovePlate3D()
		{
			const size_t count = (size_t)mNumberX * mNumberY * mNumberZ;
			for (size_t i = 0; i < count; ++i)
			{
				mPlate[i].~PlateT();
			}
			::operator delete(mPlate);
			::operator delete(LPlate);
		}

		MovePlate3D(const MovePlate3D &) = delete;
		MovePlate3D &operator=(const MovePlate3D &) = delete;

		/**
		 * @brief 获取指定索引的板块指针
		 * @warning 不进行边界检查，请确保索引有效
		 */
		[[nodiscard]] inline PlateT *PlateAt(unsigned int x, unsigned int y, unsigned int z)
		{
			return &mPlate[((size_t)x * mNumberY + y) * mNumberZ + z];
		}

		/**
		 * @brief 设置回调函数
		 */
		inline void SetCallback(_GenerateCallback G, void *GData, _DeleteCallback D, void *DData) noexcept
		{
			mGenerateCallback = G;
			mGenerateData = GData;
			mDeleteCallback = D;
			mDeleteData = DData;
		}

		/**
		 * @brief 设置监测位置
		 * @param x 世界 X 坐标（体素）
		 */
		inline void SetPos(float x, float y, float z) noexcept
		{
			mPosX = (int)(x / (float)mEdge);
			mPosY = (int)(y / (float)mEdge);
			mPosZ = (int)(z / (float)mEdge);
		}

		/**
		 * @brief 获取指定索引的板块（带边界检查）
		 * @return 越界返回 nullptr
		 */
		inline PlateT *GetPlate(int x, int y, int z) noexcept
		{
			if (x < 0 || y < 0 || z < 0 ||
			    x >= (int)mNumberX || y >= (int)mNumberY || z >= (int)mNumberZ)
			{
				return nullptr;
			}
			return PlateAt((unsigned int)x, (unsigned int)y, (unsigned int)z);
		}

		/**
		 * @brief 通过偏移量获取板块（板块坐标）
		 */
		inline PlateT *ExcursionGetPlate(int x, int y, int z) noexcept
		{
			return GetPlate(x - mPosX + (int)mOriginX, y - mPosY + (int)mOriginY,
			                z - mPosZ + (int)mOriginZ);
		}

		/**
		 * @brief 将世界坐标转换为板块指针（int 版本）
		 */
		inline PlateT *CalculateGetPlate(int x, int y, int z) noexcept
		{
			return GetPlate((x / (int)mEdge) - mPosX + (int)mOriginX,
			                (y / (int)mEdge) - mPosY + (int)mOriginY,
			                (z / (int)mEdge) - mPosZ + (int)mOriginZ);
		}

		/**
		 * @brief 将世界坐标转换为板块指针（float 版本）
		 */
		inline PlateT *CalculateGetPlate(float x, float y, float z) noexcept
		{
			return GetPlate((int)(x / (float)mEdge) - mPosX + (int)mOriginX,
			                (int)(y / (float)mEdge) - mPosY + (int)mOriginY,
			                (int)(z / (float)mEdge) - mPosZ + (int)mOriginZ);
		}

		/**
		 * @brief 更新监测位置并判断是否需要移动板块
		 * @details 移动量超过板块数量时走 ALLUpData 全量重建。
		 *          滚动顺序固定为 Z → Y → X。
		 */
		MovePlateInfo3D UpData(double x, double y, double z)
		{
			MovePlateInfo3D Info;
			Info.X = (int)(x / (double)mEdge) - mPosX;
			Info.Y = (int)(y / (double)mEdge) - mPosY;
			Info.Z = (int)(z / (double)mEdge) - mPosZ;

			if (abs(Info.X) < (int)mNumberX && abs(Info.Y) < (int)mNumberY &&
			    abs(Info.Z) < (int)mNumberZ)
			{
				// 顺序：Z → Y → X（跳过集合逐级收窄）
				if (Info.Z != 0)
				{
					mPosZ += Info.Z;
					MovePlateZ(Info.Z, Info.X, Info.Y);
					Info.UpData = true;
				}
				if (Info.Y != 0)
				{
					mPosY += Info.Y;
					MovePlateY(Info.Y, Info.X);
					Info.UpData = true;
				}
				if (Info.X != 0)
				{
					mPosX += Info.X;
					MovePlateX(Info.X);
					Info.UpData = true;
				}
				return Info;
			}

			ALLUpData(x, y, z);
			Info.UpData = true;
			return Info;
		}

		/**
		 * @brief 全量重建所有板块
		 */
		void ALLUpData(double x, double y, double z)
		{
			mPosX = (int)(x / (double)mEdge);
			mPosY = (int)(y / (double)mEdge);
			mPosZ = (int)(z / (double)mEdge);
			for (unsigned int Nx = 0; Nx < mNumberX; ++Nx)
			{
				for (unsigned int Ny = 0; Ny < mNumberY; ++Ny)
				{
					for (unsigned int Nz = 0; Nz < mNumberZ; ++Nz)
					{
						PlateT *p = PlateAt(Nx, Ny, Nz);
						if (mDeleteCallback)
							mDeleteCallback(p, mDeleteData);
						if (mGenerateCallback)
						{
							mGenerateCallback(p, (int)(Nx + mPosX - mOriginX),
							                  (int)(Ny + mPosY - mOriginY),
							                  (int)(Nz + mPosZ - mOriginZ), mGenerateData);
						}
					}
				}
			}
		}

	private:
		/**
		 * @brief 判断 X 索引是否落在「X 方向将被重新生成」的区间内
		 * @details 与 2D `MovePlateY` 的 tX/wX 判据同构。
		 */
		inline bool InXRegion(int ix, int uX) const
		{
			const int tX = uX > 0 ? uX : 0;
			const int wX = uX > 0 ? 0 : uX;
			return (ix >= tX) && (ix < ((int)mNumberX + wX));
		}

		/**
		 * @brief 沿 X 方向移动板块
		 */
		void MovePlateX(int uX)
		{
			if (uX > 0)
			{
				for (unsigned int iy = 0; iy < mNumberY; ++iy)
				{
					for (unsigned int iz = 0; iz < mNumberZ; ++iz)
					{
						for (int ix = 0; ix < uX; ++ix)
							LPlate[ix] = *PlateAt(ix, iy, iz);
						for (unsigned int ix = 0; ix < (mNumberX - uX); ++ix)
							*PlateAt(ix, iy, iz) = *PlateAt(ix + uX, iy, iz);
						for (unsigned int ix = (mNumberX - uX); ix < mNumberX; ++ix)
						{
							*PlateAt(ix, iy, iz) = LPlate[ix - (mNumberX - uX)];
							if (mDeleteCallback)
								mDeleteCallback(PlateAt(ix, iy, iz), mDeleteData);
							if (mGenerateCallback)
							{
								mGenerateCallback(PlateAt(ix, iy, iz),
								                  (int)(ix + mPosX - mOriginX),
								                  (int)(iy + mPosY - mOriginY),
								                  (int)(iz + mPosZ - mOriginZ), mGenerateData);
							}
						}
					}
				}
			}
			else
			{
				for (unsigned int iy = 0; iy < mNumberY; ++iy)
				{
					for (unsigned int iz = 0; iz < mNumberZ; ++iz)
					{
						for (int ix = 0; ix < -uX; ++ix)
							LPlate[ix] = *PlateAt(mNumberX - ix - 1, iy, iz);
						for (int ix = (int)(mNumberX + uX - 1); ix >= 0; --ix)
							*PlateAt(ix - uX, iy, iz) = *PlateAt(ix, iy, iz);
						for (int ix = 0; ix < -uX; ++ix)
						{
							*PlateAt(ix, iy, iz) = LPlate[ix];
							if (mDeleteCallback)
								mDeleteCallback(PlateAt(ix, iy, iz), mDeleteData);
							if (mGenerateCallback)
							{
								mGenerateCallback(PlateAt(ix, iy, iz),
								                  (ix + mPosX - (int)mOriginX),
								                  (iy + mPosY - (int)mOriginY),
								                  (iz + mPosZ - (int)mOriginZ), mGenerateData);
							}
						}
					}
				}
			}
		}

		/**
		 * @brief 沿 Y 方向移动板块
		 * @param uY Y 方向移动量
		 * @param uX X 方向的预测移动量（用于跳过将由 X 滚动重新生成的板块）
		 */
		void MovePlateY(int uY, int uX)
		{
			if (uY > 0)
			{
				for (unsigned int ix = 0; ix < mNumberX; ++ix)
				{
					const bool skipX = InXRegion((int)ix, uX);
					for (unsigned int iz = 0; iz < mNumberZ; ++iz)
					{
						for (int iy = 0; iy < uY; ++iy)
							LPlate[iy] = *PlateAt(ix, iy, iz);
						for (unsigned int iy = 0; iy < (mNumberY - uY); ++iy)
							*PlateAt(ix, iy, iz) = *PlateAt(ix, iy + uY, iz);
						for (unsigned int iy = (mNumberY - uY); iy < mNumberY; ++iy)
						{
							*PlateAt(ix, iy, iz) = LPlate[iy - (mNumberY - uY)];
							if (!skipX)
							{
								if (mDeleteCallback)
									mDeleteCallback(PlateAt(ix, iy, iz), mDeleteData);
								if (mGenerateCallback)
								{
									mGenerateCallback(PlateAt(ix, iy, iz),
									                  (int)(ix + mPosX - mOriginX),
									                  (int)(iy + mPosY - mOriginY),
									                  (int)(iz + mPosZ - mOriginZ), mGenerateData);
								}
							}
						}
					}
				}
			}
			else
			{
				for (unsigned int ix = 0; ix < mNumberX; ++ix)
				{
					const bool skipX = InXRegion((int)ix, uX);
					for (unsigned int iz = 0; iz < mNumberZ; ++iz)
					{
						for (int iy = 0; iy < -uY; ++iy)
							LPlate[iy] = *PlateAt(ix, mNumberY - iy - 1, iz);
						for (int iy = (int)(mNumberY + uY - 1); iy >= 0; --iy)
							*PlateAt(ix, iy - uY, iz) = *PlateAt(ix, iy, iz);
						for (int iy = 0; iy < -uY; ++iy)
						{
							*PlateAt(ix, iy, iz) = LPlate[iy];
							if (!skipX)
							{
								if (mDeleteCallback)
									mDeleteCallback(PlateAt(ix, iy, iz), mDeleteData);
								if (mGenerateCallback)
								{
									mGenerateCallback(PlateAt(ix, iy, iz),
									                  (ix + mPosX - (int)mOriginX),
									                  (iy + mPosY - (int)mOriginY),
									                  (iz + mPosZ - (int)mOriginZ), mGenerateData);
								}
							}
						}
					}
				}
			}
		}

		/**
		 * @brief 沿 Z 方向移动板块
		 * @param uZ Z 方向移动量
		 * @param uX X 方向的预测移动量
		 * @param uY Y 方向的预测移动量
		 * @details 跳过「X 区域 **或** Y 区域」内的板块：
		 *          它们会在随后的 Y 滚动 / X 滚动中被重新生成。
		 *          这是 2D 单级跳过的三维推广。
		 */
		void MovePlateZ(int uZ, int uX, int uY)
		{
			// Y 方向将被重新生成的区间（与 MovePlateY 的 tX/wX 判据同构）
			const int tY = uY > 0 ? uY : 0;
			const int wY = uY > 0 ? 0 : uY;

			auto skipYZ = [&](int ix, int iy) -> bool {
				if (InXRegion(ix, uX))
					return true;
				return (iy >= tY) && (iy < ((int)mNumberY + wY));
			};

			if (uZ > 0)
			{
				for (unsigned int ix = 0; ix < mNumberX; ++ix)
				{
					for (unsigned int iy = 0; iy < mNumberY; ++iy)
					{
						const bool skip = skipYZ((int)ix, (int)iy);
						for (int iz = 0; iz < uZ; ++iz)
							LPlate[iz] = *PlateAt(ix, iy, iz);
						for (unsigned int iz = 0; iz < (mNumberZ - uZ); ++iz)
							*PlateAt(ix, iy, iz) = *PlateAt(ix, iy, iz + uZ);
						for (unsigned int iz = (mNumberZ - uZ); iz < mNumberZ; ++iz)
						{
							*PlateAt(ix, iy, iz) = LPlate[iz - (mNumberZ - uZ)];
							if (!skip)
							{
								if (mDeleteCallback)
									mDeleteCallback(PlateAt(ix, iy, iz), mDeleteData);
								if (mGenerateCallback)
								{
									mGenerateCallback(PlateAt(ix, iy, iz),
									                  (int)(ix + mPosX - mOriginX),
									                  (int)(iy + mPosY - mOriginY),
									                  (int)(iz + mPosZ - mOriginZ), mGenerateData);
								}
							}
						}
					}
				}
			}
			else
			{
				for (unsigned int ix = 0; ix < mNumberX; ++ix)
				{
					for (unsigned int iy = 0; iy < mNumberY; ++iy)
					{
						const bool skip = skipYZ((int)ix, (int)iy);
						for (int iz = 0; iz < -uZ; ++iz)
							LPlate[iz] = *PlateAt(ix, iy, mNumberZ - iz - 1);
						for (int iz = (int)(mNumberZ + uZ - 1); iz >= 0; --iz)
							*PlateAt(ix, iy, iz - uZ) = *PlateAt(ix, iy, iz);
						for (int iz = 0; iz < -uZ; ++iz)
						{
							*PlateAt(ix, iy, iz) = LPlate[iz];
							if (!skip)
							{
								if (mDeleteCallback)
									mDeleteCallback(PlateAt(ix, iy, iz), mDeleteData);
								if (mGenerateCallback)
								{
									mGenerateCallback(PlateAt(ix, iy, iz),
									                  (ix + mPosX - (int)mOriginX),
									                  (iy + mPosY - (int)mOriginY),
									                  (iz + mPosZ - (int)mOriginZ), mGenerateData);
								}
							}
						}
					}
				}
			}
		}
	};

}
