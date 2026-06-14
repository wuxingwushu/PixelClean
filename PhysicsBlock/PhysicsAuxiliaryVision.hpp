#pragma once
#include "PhysicsWorld.hpp"

// 前向声明，避免把 Vulkan 头链带进本头文件（参考 PhysicsGPU.hpp 的轻量头模式）。
// 完整定义见 ../VulkanTool/AuxiliaryVision.h，仅在对应的 .cpp 中包含。
namespace VulKan
{
	class AuxiliaryVision;
}

namespace PhysicsBlock
{
	// ─────────────────────────────────────────────────────────────
	// 基本图元
	// ─────────────────────────────────────────────────────────────

	/**
	 * @brief 静态方块（写入静态线段缓冲，渲染到下一帧 ClearStaticLine 之前一直存在）
	 * @param pos      方块左下角坐标
	 * @param angle    旋转角度（弧度）
	 * @param color    边线颜色
	 */
	void DrawStaticSquare(VulKan::AuxiliaryVision *av, glm::dvec2 pos, double angle, glm::vec4 color);

	/**
	 * @brief 动态方块（写入本帧的动态线段缓冲，av->Begin()/End() 之间调用）
	 * @param pos      方块左下角坐标
	 * @param angle    旋转角度（弧度）
	 * @param color    边线颜色
	 */
	void DrawSquare(VulKan::AuxiliaryVision *av, glm::dvec2 pos, double angle, glm::vec4 color);

	/**
	 * @brief 物体本体默认颜色：动态物体亮绿，静态物体暗绿
	 *        （StaticNum < 10 视为动态）
	 */
	glm::vec4 ObjectBodyColor(unsigned StaticNum);

	// ─────────────────────────────────────────────────────────────
	// 单体物体：本体绘制
	// ─────────────────────────────────────────────────────────────

	/// 绘制网格形状本体（每个实体格子一个方块）
	void DrawShape(VulKan::AuxiliaryVision *av, PhysicsShape *obj, glm::vec4 color);
	/// 绘制粒子本体（一个点）
	void DrawParticle(VulKan::AuxiliaryVision *av, PhysicsParticle *obj, glm::vec4 color);
	/// 绘制圆形本体
	void DrawCircle(VulKan::AuxiliaryVision *av, PhysicsCircle *obj, glm::vec4 color);
	/// 绘制线段本体
	void DrawLine(VulKan::AuxiliaryVision *av, PhysicsLine *obj, glm::vec4 color);

	// ─────────────────────────────────────────────────────────────
	// 单体物体：辅助信息绘制（按 Auxiliary_*Bool / Auxiliary_*Color 开关逐项判断）
	// ─────────────────────────────────────────────────────────────

	void DrawShapeAuxiliary(VulKan::AuxiliaryVision *av, PhysicsShape *obj);
	void DrawParticleAuxiliary(VulKan::AuxiliaryVision *av, PhysicsParticle *obj);
	void DrawCircleAuxiliary(VulKan::AuxiliaryVision *av, PhysicsCircle *obj);
	void DrawLineAuxiliary(VulKan::AuxiliaryVision *av, PhysicsLine *obj);

	// ─────────────────────────────────────────────────────────────
	// 集合级绘制
	// ─────────────────────────────────────────────────────────────

	/// 绘制所有关节（绳子）：body1→锚点1、body2→锚点2
	void DrawJoints(VulKan::AuxiliaryVision *av, PhysicsWorld *world);
	/// 绘制所有连接点（BaseJunction）：A→B 线段
	void DrawJunctions(VulKan::AuxiliaryVision *av, PhysicsWorld *world);
	/// 绘制碰撞信息（碰撞点、分离法向量、指向重心的连线），需 Auxiliary_* 开关启用
	void DrawCollisions(VulKan::AuxiliaryVision *av, PhysicsWorld *world);
	/// 绘制所有触发器区域（橙色矩形）
	void DrawTriggers(VulKan::AuxiliaryVision *av, PhysicsWorld *world);
	/// 绘制选中物体被划分到的网格位置，需 Auxiliary_GridDividedBool 启用，selected 非空才绘制
	void DrawGridDivided(VulKan::AuxiliaryVision *av, PhysicsWorld *world, PhysicsFormwork *selected);

	/**
	 * @brief 渲染地图轮廓（静态线段）。内部会先 ClearStaticLine。
	 *        适用于 _MapStatic 与 _MapDynamic（动态地图按 PlatePos 偏移）。
	 */
	void RenderMapOutline(VulKan::AuxiliaryVision *av, PhysicsWorld *world);

	// ─────────────────────────────────────────────────────────────
	// 高层聚合：一键渲染整帧物理世界
	// ─────────────────────────────────────────────────────────────

	/**
	 * @brief 渲染物理世界的全部内容（不含 Begin/End，调用方需自行管理录制边界）。
	 * @param av              辅助视觉渲染器
	 * @param world           物理世界
	 * @param showAuxiliary   是否显示辅助信息（位置/速度/受力/碰撞点等）
	 * @param selected        当前选中的物体（用于网格划分显示），可为 nullptr
	 *
	 * 内部流程（在调用方的 av->Begin()/End() 之间执行）：
	 *   [showAuxiliary] DrawGridDivided(selected)
	 *   → 遍历四类物体：Draw*(本体) [+ showAuxiliary 时 Draw*Auxiliary]
	 *   → DrawJoints → DrawJunctions
	 *   → [showAuxiliary] DrawCollisions
	 *   → DrawTriggers
	 */
	void DrawPhysicsWorld(VulKan::AuxiliaryVision *av, PhysicsWorld *world,
						  bool showAuxiliary, PhysicsFormwork *selected = nullptr);

	/**
	 * @brief 一键渲染整帧物理世界（自包含：内部已包含 Begin/End）。
	 *        适合简单场景；若需要在同一帧的录制区间内插入其它绘制
	 *        （例如鼠标拖拽反馈线），请自行 Begin()/End() 并改用 DrawPhysicsWorld。
	 */
	void DrawPhysicsWorldFrame(VulKan::AuxiliaryVision *av, PhysicsWorld *world,
							   bool showAuxiliary, PhysicsFormwork *selected = nullptr);

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
	struct PhysicsDebugView
	{
		VulKan::AuxiliaryVision *av = nullptr;   // 辅助视觉渲染器
		PhysicsWorld            *world = nullptr; // 物理世界
		bool                     showAuxiliary = false; // 是否显示位置/速度/受力等辅助信息
		PhysicsFormwork         *selected = nullptr;   // 选中物体（用于网格划分显示），可为空
		bool                     showMap = true;        // 是否绘制地图轮廓

		// 内部状态（通常无需手动改）
		bool       mapDirty = true;       // 地图轮廓是否需要重写静态缓冲
		glm::ivec2 lastPlatePos{0, 0};    // 仅 MapDynamic：上一帧板块位置，用于自动脏检测

		/// 绑定渲染器与物理世界（构造后调用一次）
		void setup(VulKan::AuxiliaryVision *a, PhysicsWorld *w);

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

} // namespace PhysicsBlock
