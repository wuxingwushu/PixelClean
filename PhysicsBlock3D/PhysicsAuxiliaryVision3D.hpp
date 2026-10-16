#pragma once
#include "PhysicsWorld3D.hpp"
#include "RenderMode3D.hpp"

/*
 * ═══════════════════════════════════════════════════════════════════════════
 * PhysicsAuxiliaryVision3D —— 由 PhysicsBlock/PhysicsAuxiliaryVision.hpp + .cpp 移植
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * 【刻意删除的内容（3D 无对应物）】
 *   · `DrawLiquid(VulKan::AuxiliaryVision *, PhysicsLiquid *, float)` —— 3D 没有
 *     液体子系统（PhysicsLiquid3DBool3D == 0，目录内无 PhysicsLiquid3D.*），
 *     故连同前向声明 `class PhysicsLiquid;` 与所有 `#include "PhysicsLiquid*.hpp"`
 *     一并删除。将来液体落地后需要把它按密度着色的绘制补回。
 *   · 2D 的 `PhysicsWorld::GetLiquid()` 调用点同步移除。
 *
 * 【几何映射要点（与 2D 的差异）】
 *   1. 2D 的标量 angle → 3D 的 Quat_ rotation，旋转改用 `QuatRotate(rotation, v)`
 *      （BaseStruct3D.hpp:212）。世界坐标下的局部偏移量一律走该函数。
 *   2. 2D 的 `{pos, 0}`（把一切压在 z=0 平面）→ 3D 直接传 `glm::dvec3(pos)`：
 *      VulkanTool/AuxiliaryVision.h 的 Spot/Circle/AddLine/Line 全部收 `glm::dvec3`，
 *      且顶点结构（AuxiliaryLineSpot::Pos / AuxiliarySpot::Pos）本来就是
 *      glm::vec3，z 不再被浪费。
 *   3. 线段物体不再手工 `vec2angle({radius,0}, angle)` 重建端点，改用
 *      `PhysicsLine3D::GetSegment(begin, end)`（PhysicsLine3D.hpp:68），
 *      朝向/端点语义完全由物理类负责。
 *   4. 形状轮廓改为 `OutlineSet` 的 `OutlinePoint3D` 单数组
 *      （LocalPos / Normal / Friction / SeedPos / Voxel），取代 2D 的
 *      `OutlineSet` / `MaxOutlineCentreMass` / `FrictionSet` 三条平行数组。
 *   5. 体素编辑/轮廓绘制需要遍历 x,y,z 三个方向（2D 只有 x,y）。
 *
 * 【渲染后端事实】
 *   AuxiliaryVision 的**动态**绘制入口 `Line/Spot/Circle` 只能在
 *   `Begin()` / `End()` 之间调用（见 AuxiliaryVision.h:113-153 的缓冲区语义），
 *   而 `AddLine`/`AddStaticLine` 是"录制外"的一次性/静态缓冲。
 *   本文件因此把 `isStatic` 参数一路传递到内部描边函数：
 *   地图轮廓走 `AddStaticLine`（一次性写入静态缓冲，配合 ClearStaticLine），
 *   物体描边走 `Line`（每帧动态）。
 */

// 前向声明，避免把 Vulkan 头链带进本头文件（参考 PhysicsGPU.hpp 的轻量头模式）。
// 完整定义见 ../VulkanTool/AuxiliaryVision.h，仅在对应的 .cpp 中包含。
namespace VulKan
{
	class AuxiliaryVision;
}

namespace PhysicsBlock3D
{
	// ─────────────────────────────────────────────────────────────
	// 基本图元
	// ─────────────────────────────────────────────────────────────

	/**
	 * @brief 静态方块（写入静态线段缓冲，渲染到下一帧 ClearStaticLine 之前一直存在）
	 * @param pos      方块左下角（3D 下为"底面最小角"）坐标
	 * @param rotation 旋转四元数
	 * @param color    边线颜色
	 */
	void DrawStaticSquare3D(VulKan::AuxiliaryVision *av, glm::dvec3 pos, Quat_ rotation, glm::vec4 color);

	/**
	 * @brief 动态方块（写入本帧的动态线段缓冲，av->Begin()/End() 之间调用）
	 * @param pos      方块左下角坐标
	 * @param rotation 旋转四元数
	 * @param color    边线颜色
	 */
	void DrawSquare3D(VulKan::AuxiliaryVision *av, glm::dvec3 pos, Quat_ rotation, glm::vec4 color);

	/**
	 * @brief 物体本体默认颜色：动态物体亮绿，静态物体暗绿
	 *        （StaticNum < 10 视为动态，与 2D 判据一致）
	 */
	glm::vec4 ObjectBodyColor3D(unsigned StaticNum);

	// ─────────────────────────────────────────────────────────────
	// 单体物体：本体绘制
	// ─────────────────────────────────────────────────────────────

	/// 绘制网格形状本体（每个实体格子一个方块）
	void DrawShape3D(VulKan::AuxiliaryVision *av, PhysicsShape3D *obj, glm::vec4 color);
	/// 绘制粒子本体（一个点标记）
	void DrawParticle3D(VulKan::AuxiliaryVision *av, PhysicsParticle3D *obj, glm::vec4 color);
	/// 绘制球本体（线框球近似，见 .cpp 中的选择说明）
	void DrawCircle3D(VulKan::AuxiliaryVision *av, PhysicsCircle3D *obj, glm::vec4 color);
	/// 绘制线段本体（真实三维线段，端点由 GetSegment 给出）
	void DrawLine3D(VulKan::AuxiliaryVision *av, PhysicsLine3D *obj, glm::vec4 color);

	// ─────────────────────────────────────────────────────────────
	// 单体物体：辅助信息绘制（按 Auxiliary_*Bool3D / Auxiliary_*Color3D 开关逐项判断）
	// ─────────────────────────────────────────────────────────────

	void DrawShapeAuxiliary3D(VulKan::AuxiliaryVision *av, PhysicsShape3D *obj);
	void DrawParticleAuxiliary3D(VulKan::AuxiliaryVision *av, PhysicsParticle3D *obj);
	void DrawCircleAuxiliary3D(VulKan::AuxiliaryVision *av, PhysicsCircle3D *obj);
	void DrawLineAuxiliary3D(VulKan::AuxiliaryVision *av, PhysicsLine3D *obj);

	// ─────────────────────────────────────────────────────────────
	// 集合级绘制
	// ─────────────────────────────────────────────────────────────

	/// 绘制所有关节：body1→锚点1、body2→锚点2
	void DrawJoints3D(VulKan::AuxiliaryVision *av, PhysicsWorld3D *world);
	/// 绘制所有连接点（BaseJunction3D）：A→B 线段
	void DrawJunctions3D(VulKan::AuxiliaryVision *av, PhysicsWorld3D *world);
	/// 绘制碰撞信息（碰撞点、分离法向量、指向重心的连线），需 Auxiliary_*Bool3D 开关启用
	void DrawCollisions3D(VulKan::AuxiliaryVision *av, PhysicsWorld3D *world);
	/// 绘制所有触发器区域（橙色三维 AABB 线框）
	void DrawTriggers3D(VulKan::AuxiliaryVision *av, PhysicsWorld3D *world);
	/// 绘制选中物体被划分到的网格位置（三维盒线框），需 Auxiliary_GridDividedBool3D 启用
	void DrawGridDivided3D(VulKan::AuxiliaryVision *av, PhysicsWorld3D *world, PhysicsFormwork3D *selected);

	/**
	 * @brief 渲染地图轮廓（静态线段）。内部会先 ClearStaticLine。
	 *        适用于 _MapStatic 与 _MapDynamic（动态地图按 PlatePos 偏移）。
	 */
	void RenderMapOutline3D(VulKan::AuxiliaryVision *av, PhysicsWorld3D *world);

	// ─────────────────────────────────────────────────────────────
	// 高层聚合：一键渲染整帧物理世界
	// ─────────────────────────────────────────────────────────────

	/**
	 * @brief 渲染物理世界的全部内容（不含 Begin/End，调用方需自行管理录制边界）。
	 * @param av              辅助视觉渲染器
	 * @param world           物理世界
	 * @param showAuxiliary   是否显示辅助信息（位置/速度/受力/碰撞点等）
	 * @param selected        当前选中的物体（用于网格划分显示），可为 nullptr
	 * @param modes           渲染方式状态；**处于"面"渲染的物体/地形会跳过线框绘制**
	 *                        （它的面由 `PhysicsFaceRenderer3D` 画），nullptr = 全部线框
	 *
	 * 内部流程（在调用方的 av->Begin()/End() 之间执行）：
	 *   [showAuxiliary] DrawGridDivided3D(selected)
	 *   → 遍历四类物体：Draw*3D(本体) [+ showAuxiliary 时 Draw*3DAuxiliary]
	 *   → DrawJoints3D → DrawJunctions3D
	 *   → [showAuxiliary] DrawCollisions3D
	 *   → DrawTriggers3D
	 */
	void DrawPhysicsWorld3D(VulKan::AuxiliaryVision *av, PhysicsWorld3D *world,
							bool showAuxiliary, PhysicsFormwork3D *selected = nullptr,
							const RenderModeState3D *modes = nullptr);

	/**
	 * @brief 一键渲染整帧物理世界（自包含：内部已包含 Begin/End）。
	 *        适合简单场景；若需要在同一帧的录制区间内插入其它绘制
	 *        （例如鼠标拖拽反馈线），请自行 Begin()/End() 并改用 DrawPhysicsWorld3D。
	 */
	void DrawPhysicsWorldFrame3D(VulKan::AuxiliaryVision *av, PhysicsWorld3D *world,
								 bool showAuxiliary, PhysicsFormwork3D *selected = nullptr,
								 const RenderModeState3D *modes = nullptr);

	// ─────────────────────────────────────────────────────────────
	// 物理辅助显示控制器
	// 把"地图轮廓（静态缓冲）+ 物理世界（动态绘制）+ 脏检测 + UI"
	// 这整套状态协调封装进一个对象。典型用法：
	//   构造时：mPhysicsDebug.setup(mAuxiliaryVision, mSquarePhysics);
	//   每帧  ：mPhysicsDebug.refreshMap();         // 在 mAuxiliaryVision->Begin() 之前
	//           mAuxiliaryVision->Begin();
	//           ... 其它本帧动态绘制（鼠标拖拽反馈线、寻路调试点等）...
	//           mPhysicsDebug.drawWorld();          // 在 Begin()/End() 之间
	//           mAuxiliaryVision->End();
	//   UI    ：mPhysicsDebug.drawUI();             // GameUI() 里
	// 若模块没有自己的内联辅助绘制，可直接用 render() 一步到位（内部含 Begin/End）。
	// ─────────────────────────────────────────────────────────────
	struct PhysicsDebugView3D
	{
		VulKan::AuxiliaryVision *av = nullptr;			 // 辅助视觉渲染器
		PhysicsWorld3D			*world = nullptr;		 // 物理世界
		bool					 showAuxiliary = false;	 // 是否显示位置/速度/受力等辅助信息
		PhysicsFormwork3D		*selected = nullptr;	 // 选中物体（用于网格划分显示），可为空
		bool					 showMap = true;		 // 是否绘制地图轮廓
		const RenderModeState3D *modes = nullptr;		 // 渲染方式状态（面模式物体跳过线框），可为空

		// 内部状态（通常无需手动改）
		bool	 mapDirty = true;			 // 地图轮廓是否需要重写静态缓冲
		IVec3_	 lastPlatePos{0, 0, 0};		 // 仅 MapDynamic3D：上一帧板块位置，用于自动脏检测

		/// 绑定渲染器与物理世界（构造后调用一次）
		void setup(VulKan::AuxiliaryVision *a, PhysicsWorld3D *w);

		/// 刷新地图轮廓（静态缓冲）。每帧在 mAuxiliaryVision->Begin() 之前调用。
		/// 内部做静态/动态脏检测：动态地图板块移动时自动重写，静态地图仅在 mapDirty 时写一次。
		void refreshMap();

		/// 绘制物理世界（动态）。需在调用方的 mAuxiliaryVision->Begin()/End() 之间调用。
		void drawWorld();

		/// 一步到位：refreshMap() + Begin() + drawWorld() + End()。
		/// 适合没有自己内联辅助绘制的简单模块；否则用上面的分步形式。
		void render();

		/// ImGui 控件：开启/关闭辅助视觉 + 显示地图轮廓复选框 + PhysicsAuxiliaryColorUI。
		/// 需要在 ImGui 上下文已就绪时调用（即 GameUI() 里）。
		void drawUI(const char *windowTitle = u8"物理辅助显示");
	};

} // namespace PhysicsBlock3D
