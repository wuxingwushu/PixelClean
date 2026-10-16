#pragma once
#include "MapFormwork3D.hpp"
#include "BaseGrid3D.hpp"
#include "MovePlate3D.h"
#include "BaseDefine3D.h"
#include "BaseCalculate3D.hpp"
#include <assert.h>

namespace PhysicsBlock3D
{

	/**
	 * @brief 动态三维地图
	 * @details 由 2D `MapDynamic`（414 行 .cpp / 252 行 .hpp）改写。
	 *
	 * 结构：地图由 width×height×depth 个「板块」组成，每个板块是一个
	 * `PixelBlockEdgeSize3D` 立方的 `BaseGrid3D`（默认 16³）。板块由
	 * `MovePlate3D<BaseGrid3D*>` 管理，随观察者位置滚动复用，只有进入视野的
	 * 板块才会被生成回调填充数据。
	 *
	 * 维度映射：
	 *  - `width/height` → `width/height/depth`（板块数量）
	 *  - `MovePlate<BaseGrid*>` → `MovePlate3D<BaseGrid3D*>`
	 *  - `at(x,y)` → `at(x,y,z)`，板块内偏移 `x&0xF, y&0xF` → `+ z&0xF`
	 *  - `centrality` `Vec2_` → `Vec3_`
	 *  - 贪心方块分配（Morton2D）→ 贪心立方体分配（Morton3D）
	 *  - `FMBresenhamDetection` → `FMDDADetection`（统一走 `BaseGrid3D::DDADetection`，
	 *    它内部的 `at()` 是虚函数，会自动派发到本类的板块查找）
	 *
	 * 相对 2D 的修正：
	 *  1. 2D `MapDynamic` 自带一份 Bresenham，判据（`>=`/`<=`）与 `BaseGrid` 版
	 *     不一致，行为分叉；3D 统一复用 `BaseGrid3D::DDADetection`。
	 *  2. 2D 的 `at()` 未命中时返回 `static GridBlock emptyBlock{}`（可写共享静态，
	 *     写它会污染所有后续查询）；3D 保留该行为但加注释警示。
	 *  3. 2D 构造/析构 `new char[]` 配 `delete`（非 `delete[]`）不匹配；3D 用
	 *     `::operator new` / `::operator delete` 配对。
	 */
	class MapDynamic3D : public MapFormwork3D, public BaseGrid3D
	{
	public:
		/**
		 * @brief 板块生成回调
		 * @param mT 板块指针（指向 `BaseGrid3D*` 槽位）
		 * @param x/y/z 板块的世界板块坐标
		 */
		typedef void (*_MapDynamicGenerateCallback3D)(BaseGrid3D **mT, int x, int y, int z, void *Data);

		/**
		 * @brief 板块销毁回调
		 */
		typedef void (*_MapDynamicDeleteCallback3D)(BaseGrid3D **mT, void *Data);

	private:
		/** @brief 板块 X 方向数量 */
		const unsigned int width;
		/** @brief 板块 Y 方向数量 */
		const unsigned int height;
		/** @brief 板块 Z 方向数量 */
		const unsigned int depth;

		/** @brief 中心位置（世界坐标，体素） */
		Vec3_ centrality{0.0f};

		/** @brief 板块移动控制台 */
		MovePlate3D<BaseGrid3D *> mMovePlate;

		/** @brief 所有板块网格（placement new 数组） */
		BaseGrid3D *BaseGridBuffer;

		/** @brief 每个板块网格的体素数据 */
		GridBlock3D *GridBuffer;

		/**
		 * @brief 获取板块槽位（按板块坐标 + 偏移）
		 */
		BaseGrid3D **atGrid(int x, int y, int z) { return mMovePlate.ExcursionGetPlate(x, y, z); }
		BaseGrid3D **atGrid(FLOAT_ x, FLOAT_ y, FLOAT_ z)
		{
			return mMovePlate.CalculateGetPlate(x, y, z);
		}
		BaseGrid3D **atGrid(IVec3_ pos) { return atGrid(pos.x, pos.y, pos.z); }
		BaseGrid3D **atGrid(Vec3_ pos) { return atGrid(pos.x, pos.y, pos.z); }

	public:
		/**
		 * @brief 构造函数
		 * @param Width 板块 X 方向数量
		 * @param Height 板块 Y 方向数量
		 * @param Depth 板块 Z 方向数量
		 * @note 单位：板块。每个板块边长 `PixelBlockEdgeSize3D` 体素。
		 */
		MapDynamic3D(const unsigned int Width, const unsigned int Height, const unsigned int Depth);
		~MapDynamic3D();

		MapDynamic3D(const MapDynamic3D &) = delete;
		MapDynamic3D &operator=(const MapDynamic3D &) = delete;

		/*=========BaseGrid3D=========*/

		/**
		 * @brief 获取格子（按体素坐标，跨板块自动查找）
		 * @return 命中的格子；未命中（板块槽位为空）时返回共享静态空块
		 * @warning 未命中时返回的静态空块是**可写共享**的，写入会污染后续查询。
		 *          2D 版同样如此，3D 保留语义并显式警示。
		 */
		virtual GridBlock3D &at(int x, int y, int z) override
		{
			BaseGrid3D **platePtr = mMovePlate.ExcursionGetPlate(
			    x >> PixelBlockPowerMaxNum3D, y >> PixelBlockPowerMaxNum3D,
			    z >> PixelBlockPowerMaxNumZ3D);
			if (platePtr && *platePtr)
			{
				return (*platePtr)->at(x & PixelBlockPowerMinNum3D, y & PixelBlockPowerMinNum3D,
				                       z & PixelBlockPowerMinNumZ3D);
			}
			static GridBlock3D emptyBlock{};
			return emptyBlock;
		}

		virtual GridBlock3D &at(IVec3_ pos) override { return at(pos.x, pos.y, pos.z); }

		GridBlock3D &at(FLOAT_ x, FLOAT_ y, FLOAT_ z)
		{
			return at(ToInt(x), ToInt(y), ToInt(z));
		}

		GridBlock3D &at(Vec3_ pos) { return at(ToInt(pos)); }

		/**
		 * @brief 更新观察者位置（滚动板块）
		 * @param pos 新的世界位置（体素）
		 * @return 板块移动信息
		 */
		MovePlateInfo3D Updata(Vec3_ pos)
		{
			return mMovePlate.UpData(pos.x, pos.y, pos.z);
		}

		/**
		 * @brief 设置板块生成 / 销毁回调
		 */
		inline void SetCallback(_MapDynamicGenerateCallback3D G, void *GData,
		                        _MapDynamicDeleteCallback3D D, void *DData) noexcept
		{
			mMovePlate.SetCallback(G, GData, D, DData);
		}

		/**
		 * @brief 设置初始监测位置（世界坐标，体素）
		 */
		inline void SetPos(float x, float y, float z) noexcept
		{
			mMovePlate.SetPos(x, y, z);
		}

		/**
		 * @brief 全局刷新所有板块
		 */
		inline void ALLUpData(float x, float y, float z) noexcept
		{
			mMovePlate.ALLUpData(x, y, z);
		}

		/**
		 * @brief 获取当前所在板块坐标
		 */
		inline IVec3_ GetPlatePos() const noexcept
		{
			return IVec3_(mMovePlate.GetPlateX(), mMovePlate.GetPlateY(), mMovePlate.GetPlateZ());
		}

		/**
		 * @brief 获取板块边长（体素）
		 */
		inline unsigned int GetPlateEdgeSize() const noexcept { return PixelBlockEdgeSize3D; }

		inline unsigned int GetPlateCountX() const noexcept { return width; }
		inline unsigned int GetPlateCountY() const noexcept { return height; }
		inline unsigned int GetPlateCountZ() const noexcept { return depth; }

		/**
		 * @brief 提取区域轮廓（坐标为本地图体素坐标，不含 centrality 偏移）
		 */
		void GetLightweightOutline(int x_, int y_, int z_, int w_, int h_, int d_,
		                           std::vector<MapOutline3D> &Outline);
		void GetOutline(int x_, int y_, int z_, int w_, int h_, int d_,
		                std::vector<MapOutline3D> &Outline);
		void GetMinOutline(int x_, int y_, int z_, int w_, int h_, int d_,
		                   std::vector<MapOutline3D> &Outline);

		/*=========MapFormwork3D=========*/

		/**
		 * @brief 地图模拟场大小（体素）
		 */
		virtual UVec3_ FMGetMapSize() override
		{
			return UVec3_(width * PixelBlockEdgeSize3D, height * PixelBlockEdgeSize3D,
			              depth * PixelBlockEdgeSizeZ3D);
		}

		virtual bool FMGetCollide(IVec3_ start) override
		{
			return at(start.x, start.y, start.z).Collision;
		}

		virtual bool FMGetCollide(Vec3_ start) override { return at(start).Collision; }

		/**
		 * @brief 获取网格（不安全）
		 */
		virtual GridBlock3D &FMGetGridBlock(IVec3_ start) override
		{
			return at(start.x, start.y, start.z);
		}

		/**
		 * @brief 三维 DDA（体素坐标）
		 * @details 直接复用 `BaseGrid3D::DDADetection`：其内部 `at()` 为虚函数，
		 *          会自动派发到本类的板块查找，无需另写一份遍历。
		 */
		virtual CollisionInfoI3D FMDDADetection(IVec3_ start, IVec3_ end) override
		{
			return BaseGrid3D::DDADetection(start, end);
		}

		/**
		 * @brief 三维 DDA（世界坐标）
		 * @details 动态地图的 centrality 恒为 {0,0,0}，故与体素坐标一致。
		 */
		virtual CollisionInfoD3D FMDDADetection(Vec3_ start, Vec3_ end) override
		{
			return BaseGrid3D::DDADetection(start, end);
		}

		virtual CollisionInfoI3D FMSafeDDADetection(IVec3_ start, IVec3_ end) override;

		virtual Vec3_ FMGetCentrality() override { return Vec3_(0.0f); }

		virtual void FMGetMinOutline(int x_, int y_, int z_, int w_, int h_, int d_,
		                             std::vector<MapOutline3D> &Outline) override
		{
			GetMinOutline(x_, y_, z_, w_, h_, d_, Outline);
		}

		virtual void FMGetLightweightOutline(int x_, int y_, int z_, int w_, int h_, int d_,
		                                     std::vector<MapOutline3D> &Outline) override
		{
			GetLightweightOutline(x_, y_, z_, w_, h_, d_, Outline);
		}

		virtual void FMGetOutline(int x_, int y_, int z_, int w_, int h_, int d_,
		                          std::vector<MapOutline3D> &Outline) override
		{
			GetOutline(x_, y_, z_, w_, h_, d_, Outline);
		}

		virtual bool SafeSetCollision(IVec3_ pos, bool state) override;
		virtual void SetFriction(int x, int y, int z, FLOAT_ friction) override;
		virtual FLOAT_ GetFriction(int x, int y, int z) override;
	};

}
