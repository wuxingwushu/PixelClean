#pragma once
#include "BaseStruct3D.hpp"
#include "PhysicsBlockTypes3D.hpp"
#include <vector>
#include <functional>

namespace PhysicsBlock3D
{

	/**
	 * @brief 三维地图轮廓结构体
	 * @details 由 2D 的 `MapOutline{Vec2_ pos, Vec2_ face, FLOAT_ F}` 改写而来。
	 *
	 * 2D 语义（MapStatic.cpp:91-243）：`pos` 是轮廓点（含 centrality 偏移），
	 * `face` 是 2×2 的角点方向（分量取 ±1），窄相用
	 * `beginDian = d.pos - d.face * 0.5` 从轮廓点向体内偏半个体素得到"面的内点"，
	 * 再与线段求交。3D 的 `face` 直接改为**外向面法线**（6 个轴向单位向量之一），
	 * 窄相用 `d.pos - d.face * 0.5f` 得到体内侧的面中心，语义完全一致且更明确。
	 */
	struct MapOutline3D
	{
		/** @brief 轮廓点位置（世界坐标，含 centrality 偏移） */
		Vec3_ pos;

		/** @brief 该点的外向面法线（6 个轴向单位向量之一） */
		Vec3_ face;

		/** @brief 该点的摩擦因数 */
		FLOAT_ F;

		MapOutline3D() : pos(0.0f), face(0.0f, 1.0f, 0.0f), F(0.2f) {}

		MapOutline3D(Vec3_ Pos, Vec3_ Face, FLOAT_ Friction)
		    : pos(Pos), face(Face), F(Friction) {}
	};

	/**
	 * @brief 三维地图模板接口
	 * @details 由 2D `MapFormwork`（156 行）逐函数改写，维度映射：
	 *  - `glm::uvec2 FMGetMapSize()`      → `UVec3_ FMGetMapSize()`
	 *  - `Vec2_ FMGetCentrality()`        → `Vec3_ FMGetCentrality()`
	 *  - `FMGetCollide(glm::ivec2)`       → `FMGetCollide(IVec3_)`
	 *  - `FMGetCollide(Vec2_)`            → `FMGetCollide(Vec3_)`
	 *  - `FMGetGridBlock(glm::ivec2)`     → `FMGetGridBlock(IVec3_)`
	 *  - `FMGetXxxOutline(x,y,w,h,...)`   → `FMGetXxxOutline(x,y,z,w,h,d,...)`
	 *  - `FMBresenhamDetection(...)`      → `FMDDADetection(...)`（三维 DDA 取代 2D 直线光栅）
	 *  - `FMSafeBresenhamDetection(...)`  → `FMSafeDDADetection(...)`
	 *  - `SafeSetCollision(glm::ivec2,b)` → `SafeSetCollision(IVec3_,b)`
	 *  - `SetFriction(int,int,f)`         → `SetFriction(int,int,int,f)`
	 *  - `GetFriction(int,int)`           → `GetFriction(int,int,int)`
	 *  - 变更通知 `glm::ivec2`            → `IVec3_`
	 *
	 * @note 2D 的两个 `FMGetCollide` 重载有**语义差异**（既有的坑）：
	 *       `FMGetCollide(Vec2_)` 内部会加 `centrality` 再转 ivec2，
	 *       而 `FMGetCollide(glm::ivec2)` 不加。窄相（PhysicsBaseCollide.cpp:380-414）
	 *       必须用浮点重载，否则世界坐标为负时检测完全失效。
	 *       3D 版沿用同一约定并加注释显式警示。
	 */
	class MapFormwork3D
	{
	public:
		PhysicsObjectEnum type = PhysicsObjectEnum::Null;

		MapFormwork3D() {}
		virtual ~MapFormwork3D() {}

		/**
		 * @brief 获取对象类型
		 */
		PhysicsObjectEnum FMGetType() { return type; }

		/**
		 * @brief 获取距离场（最小碰撞半径）
		 */
		virtual unsigned char FMGetDistanceField() { return 0; }

		/**
		 * @brief 获取地图尺寸（体素单位）
		 */
		virtual UVec3_ FMGetMapSize() { return UVec3_(0); }

		/**
		 * @brief 获取地图质心偏移
		 * @details 世界坐标 = 体素坐标 + centrality。静态地图把 centrality 设为
		 *          半尺寸以使世界原点落在地图中心；动态地图返回 0。
		 */
		virtual Vec3_ FMGetCentrality() { return Vec3_(0.0f); }

		/**
		 * @brief 查询体素碰撞标志（体素坐标，**不加 centrality**）
		 */
		virtual bool FMGetCollide(IVec3_ pos);

		/**
		 * @brief 查询体素碰撞标志（世界坐标，**内部会加 centrality**）
		 * @warning 窄相请务必使用本重载。用 `ToInt(pos)` 走体素重载会漏掉
		 *          centrality 偏移，世界坐标为负时检测失效（2D 既有缺陷）。
		 */
		virtual bool FMGetCollide(Vec3_ pos);

		/**
		 * @brief 获取体素块引用
		 */
		virtual GridBlock3D &FMGetGridBlock(IVec3_ pos) = 0;

		/**
		 * @brief 提取区域轻量轮廓
		 */
		virtual void FMGetLightweightOutline(int x_, int y_, int z_, int w_, int h_, int d_,
		                                     std::vector<MapOutline3D> &Outline) {}

		/**
		 * @brief 提取区域完整轮廓
		 */
		virtual void FMGetOutline(int x_, int y_, int z_, int w_, int h_, int d_,
		                          std::vector<MapOutline3D> &Outline) {}

		/**
		 * @brief 提取区域最小轮廓
		 * @details 只返回每个方向上的极值支撑点，点数最少。
		 *          窄相（Shape-Map / Circle-Map / Line-Map）用它做精检。
		 */
		virtual void FMGetMinOutline(int x_, int y_, int z_, int w_, int h_, int d_,
		                             std::vector<MapOutline3D> &Outline) {}

		/**
		 * @brief 三维 DDA 射线检测（体素坐标）
		 */
		virtual CollisionInfoI3D FMDDADetection(IVec3_ start, IVec3_ end);

		/**
		 * @brief 三维 DDA 射线检测（世界坐标）
		 * @details 纯虚：任何具体地图都能直接转发到 `BaseGrid3D::DDADetection`，
		 *          但接口层无法假定内部存储形式，故强制派生类实现。
		 */
		virtual CollisionInfoD3D FMDDADetection(Vec3_ start, Vec3_ end) = 0;

		/**
		 * @brief 带边界保护的三维 DDA（体素坐标）
		 */
		virtual CollisionInfoI3D FMSafeDDADetection(IVec3_ start, IVec3_ end);

		/**
		 * @brief 安全设置体素碰撞标志
		 * @return 是否成功（越界返回 false）
		 */
		virtual bool SafeSetCollision(IVec3_ pos, bool collision) = 0;

		/**
		 * @brief 设置体素摩擦因数
		 */
		virtual void SetFriction(int x, int y, int z, FLOAT_ friction) = 0;

		/**
		 * @brief 获取体素摩擦因数
		 */
		virtual FLOAT_ GetFriction(int x, int y, int z) = 0;

		/**
		 * @brief 注册碰撞变更通知（供 GridSearch/渲染层增量刷新）
		 */
		void SetCollisionChangeNotifier(std::function<void(IVec3_, bool)> notifier)
		{
			mCollisionChangeNotifier = std::move(notifier);
		}

		/**
		 * @brief 注销碰撞变更通知
		 */
		void ClearCollisionChangeNotifier() { mCollisionChangeNotifier = nullptr; }

	protected:
		/** @brief 碰撞变更通知回调 */
		std::function<void(IVec3_, bool)> mCollisionChangeNotifier;

		/**
		 * @brief 触发碰撞变更通知
		 */
		inline void NotifyCollisionChange(IVec3_ pos, bool collision)
		{
			if (mCollisionChangeNotifier)
			{
				mCollisionChangeNotifier(pos, collision);
			}
		}
	};

}
