#pragma once
#include <unordered_map>
#include "PhysicsFormwork3D.hpp"

/*
 * ═══════════════════════════════════════════════════════════════════════════
 * PhysicsBlock3D 渲染方式（线 / 面）
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * 需求（2026-10，用户）：3D 物理测试的**物体**与**地形**都要支持两种渲染方式——
 *   · 线：当前的线框轮廓（`PhysicsAuxiliaryVision3D` 的 Line 管线）；
 *   · 面：实心面，观感与 `GameMods/BlockWorld` 一致（复用它的
 *         `PipelineMods::BlockWorldMods` 三角形管线与法线光照）。
 * 两种方式可以整体直接切换，也可以**单独针对某个物体或地形**切换。
 *
 * 因此把"谁用什么方式"抽成这里的状态：一个全局默认值 + 地形单独值 + 单物体覆盖。
 * 判定优先级：单物体覆盖 > 地形单独值（仅地形）> 全局默认值。
 *
 * 注意：本文件只描述**状态**，不含任何 Vulkan 代码——这样物理库的
 * `DrawPhysicsWorld3D` 也能读取它（用来跳过"面模式物体"的线框绘制），
 * 而真正的面渲染（顶点缓冲/指令录制）放在游戏层的
 * `GameMods/PhysicsTest3D/PhysicsFaceRenderer3D`。
 */

namespace PhysicsBlock3D
{

	/**
	 * @brief 物体/地形的渲染方式
	 */
	enum class RenderMode3D : int
	{
		Line = 0, // 线框轮廓（默认，与 2D 版一致）
		Face = 1, // 实心面（BlockWorld 同款三角形管线 + 法线光照）
	};

	/**
	 * @brief 渲染方式状态：全局默认 + 地形单独 + 单物体覆盖
	 */
	struct RenderModeState3D
	{
		/** @brief 全局默认渲染方式（未单独设置的物体/地形都用它） */
		RenderMode3D global = RenderMode3D::Line;

		/** @brief 地形（地图样板轮廓）单独设置的值 */
		RenderMode3D terrain = RenderMode3D::Line;

		/** @brief 地形是否脱离全局默认（false = 跟随全局） */
		bool terrainOverride = false;

		/**
		 * @brief 单物体覆盖表
		 * @warning 键是物体指针。物体删除后必须 `ClearObjectMode`，否则表里会
		 *          残留悬垂指针；析构/切换场景时用 `ClearAllObjectModes` 清空。
		 */
		std::unordered_map<PhysicsFormwork3D *, RenderMode3D> objectOverride;

		/** @brief 地形的最终渲染方式 */
		RenderMode3D TerrainMode() const { return terrainOverride ? terrain : global; }

		/** @brief 该物体是否被单独设置过 */
		bool HasObjectOverride(PhysicsFormwork3D *o) const
		{
			return o != nullptr && objectOverride.find(o) != objectOverride.end();
		}

		/** @brief 物体的最终渲染方式 */
		RenderMode3D ObjectMode(PhysicsFormwork3D *o) const
		{
			if (o == nullptr)
				return global;
			const auto it = objectOverride.find(o);
			return it == objectOverride.end() ? global : it->second;
		}

		/** @brief 单独设置某物体的渲染方式 */
		void SetObjectMode(PhysicsFormwork3D *o, RenderMode3D m)
		{
			if (o != nullptr)
				objectOverride[o] = m;
		}

		/** @brief 取消某物体的单独设置（回到跟随全局） */
		void ClearObjectMode(PhysicsFormwork3D *o)
		{
			if (o != nullptr)
				objectOverride.erase(o);
		}

		/** @brief 清空所有单物体设置 */
		void ClearAllObjectModes() { objectOverride.clear(); }

		/**
		 * @brief 该类物体是否支持面渲染
		 * @details 只有体素网格（`PhysicsShape3D`）与球（`PhysicsCircle3D`）有实体外观；
		 *          线段与点没有"体面"，恒以线框/点标记显示——即使给它设了 Face
		 *          （`ObjectMode` 返回 Face）也只按线框画，避免出现"一半场景实体化
		 *          但线/点凭空消失"的观感。
		 */
		static bool SupportsFace(PhysicsFormwork3D *o)
		{
			if (o == nullptr)
				return false;
			const PhysicsObjectEnum t = o->PFGetType();
			return t == PhysicsObjectEnum::shape || t == PhysicsObjectEnum::circle;
		}
	};

} // namespace PhysicsBlock3D
