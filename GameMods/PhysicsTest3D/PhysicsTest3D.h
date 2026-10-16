#pragma once
#include "../Configuration.h"
#include "../GameMods.h"
#include "../../PhysicsBlock3D/PhysicsWorld3D.hpp"
#include "../../PhysicsBlock3D/MapStatic3D.hpp"
#include "../../PhysicsBlock3D/MapDynamic3D.hpp"
#include "../../PhysicsBlock3D/PhysicsLiquid3D.hpp"
#include "../../PhysicsBlock3D/PhysicsAuxiliaryVision3D.hpp"
#include "../../PhysicsBlock3D/RenderMode3D.hpp"
#include "PhysicsFaceRenderer3D.hpp"
#include "PhysicsLogBuffer3D.h"
#include "PhysicsDemo3D.h"

/*
 * ═══════════════════════════════════════════════════════════════════════════
 * GAME::PhysicsTest3D —— 3D 体素刚体物理引擎的游戏层沙盒外壳
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * 由 `GameMods/PhysicsTest/PhysicsTest.{h,cpp}`（2D，1441 行）逐方法改写。
 * 结构、交互流程、成员命名都刻意与 2D 版一一对应，便于对照审阅。
 *
 * ── 与 2D 版的**必须**差异（逐条对应任务书）────────────────────────────
 *
 *  1. **坐标系**：`Vec2_` → `Vec3_`。2D 把一切压在 z=0 平面、相机固定在
 *     `z=50` 俯视；3D 改为**自由飞行相机**（yaw/pitch/速度），写法照抄
 *     `GameMods/BlockWorld`：`front = (cos(pitch)sin(yaw), sin(pitch),
 *     cos(pitch)cos(yaw))`，`Yaw/Pitch` 单位为度。
 *
 *  2. **没有"画面平面"**：2D 用 `get_ray_direction(...)` 后
 *     `*= -camPos.z/dir.z` 把射线打到 z=0 平面取鼠标世界点（`PhysicsTest.cpp`
 *     的 `mMouseWorldPos`）。3D 改为真正的**射线拾取**，见
 *     `PickWorldPoint3D()`：`mCamera->screenToWorld(CursorPosX, CursorPosY, w, h)`
 *     返回的是**归一化方向向量**（`Camera.cpp:69-81`），从
 *     `mCamera->getCameraPos()` 沿该方向以 0.25 步长步进（最多 400 步），
 *     每步用 `PhysicsWorld3D::Get(point)` 命中物体、用体素实体位命中地图。
 *     未命中则回落到地面平面 `y = 0` 求交（等价于 2D 的 z=0 平面）。
 *
 *  3. **体素编辑三维化**：2D 的 `std::map<std::pair<int,int>, GridBlock>`
 *     网格编辑器（`EnterGridEdit`/`GridEditPaint`/`RenderGridEditUI`/
 *     `RenderGridCellMenu`）在 3D 下**大幅简化**，见 .cpp 里 `EnterGridEdit`
 *     上方的注释块。简化后的能力：选中一个 `PhysicsShape3D`（**动态地图也
 *     支持**）后，用一个 ImGui 滑块选定 Z 层，鼠标拾取点换算成该层的
 *     局部 (X,Y)，左键笔刷切换该体素的 Entity/Collision，右键笔刷擦除；
 *     单个体素的完整调试（质量/摩擦/血量/距离场/方向场）直接复用
 *     `ImGuiPhysics3D.hpp` 的 `GridImGui3D` 控件。**没有**迁移 2D 的
 *     "从地图抠出一块进编辑器"、"整块 map 快照回滚"、"单元格右键菜单"。
 *
 *  4. **GPU 后端明确排除**：删掉了 2D 的 `mPhysicsGPU` /
 *     `mUseGPUApplyImpulse` / `mGPUInitialized` / `mGPULastFrameTime` /
 *     `mCPULastFrameTime` 以及全部相关 UI。3D 引擎没有 GPU 路径
 *     （`BaseDefine3D.h` 注释里明确排除 PhysicsGPU），CPU 用时改由引擎的
 *     公开计时访问器给出（`GetCollisionDetectionTimeMS()` /
 *     `GetPreStepTimeMS()` / `GetApplyImpulseCPUTimeMS()` /
 *     `GetPositionUpdateTimeMS()` / `GetPostProcessTimeMS()`）。
 *
 *  5. **液体**：3D 的 `PhysicsLiquid3D` 已可用（`PhysicsLiquid3DBool3D == 1`），
 *     世界后处理阶段自动 `Update`，demo 23 使用。外壳只在 UI 里显示
 *     液体粒子数与平均密度；**渲染层不画液体**（`PhysicsAuxiliaryVision3D`
 *     刻意删除了 `DrawLiquid`，见其头文件顶部注释），故液体只以
 *     "粒子数/密度"文本可见。
 *
 *  6. **物理步长**：沿用 2D 的 `PhysicsTick = 1.0 / 100.0` 累加器写法。
 *
 *  7. **动态地图**：2D 的 `MovePlateInfo` / `MapDynamic::Updata(Vec2_)` →
 *     3D 的 `MovePlateInfo3D` / `MapDynamic3D::Updata(Vec3_)`；
 *     返回字段是 `{bool UpData; int X, Y, Z;}`（`MovePlate3D.h:13-19`），
 *     判据由 2D 的 `bool` 返回值改为读 `.UpData`。
 *
 * ── 析构顺序的特别注意（任务书要求写清理由）────────────────────────────
 *  2D 版析构是 `delete mAuxiliaryVision; delete mPhysicsWorld;`。
 *  **3D 版必须反过来：先 `delete mPhysicsWorld`，再 `delete mAuxiliaryVision`。**
 *  理由：`RenderMapOutline3D(av, world)` 会把地图轮廓写进 **av 的静态线段
 *  缓冲区**；`DrawPhysicsWorld3D(av, world, selected)` 也会在运行时解引用
 *  `world` 里的物体。若先删 av 再删 world，两者之间没有任何解引用（2D 恰好
 *  安全），但 av 的静态缓冲里仍保存着**指向已删除世界的语义**（世界重建后
 *  轮廓必须重写）；而反过来"先删世界"时，av 只是持有一份线段顶点数据、
 *  不持有 world 的对象指针，所以是安全的。为消除任何"av 里残留世界指针"
 *  的悬垂疑虑，构造函数里我们把 world 的指针交给 av 之前先做一次
 *  `RenderMapOutline3D`，析构时按 **世界 → 视觉** 顺序释放。
 */

namespace GAME
{

	class PhysicsTest3D : public GameMods, Configuration
	{
	public:
		/**
		 * @brief 构造函数
		 * @param wConfiguration 引擎装配好的配置（mDevice/mSwapChain/mRenderPass/
		 *        mPipelineS/mCamera/mWindow/mCameraVPMatricesBuffer/InterFace 等）
		 * @note 签名与 2D 的 `PhysicsTest(Configuration wConfiguration)` 完全一致，
		 *       `application.cpp` 的 `new PhysicsTest3D(*this)` 依赖它。
		 */
		PhysicsTest3D(Configuration wConfiguration);

		/**
		 * @brief 析构函数
		 * @note 释放顺序：物理世界 → 地图 → 辅助视觉（理由见文件头注释）
		 */
		~PhysicsTest3D();

		// ── GameMods 接口 ──────────────────────────────────────────────

		/** @brief 鼠标移动事件（拾取 + 相机旋转 + 拖拽） */
		virtual void MouseMove(double xpos, double ypos) override;

		/** @brief 鼠标滚轮事件（相机速度 + 创建物体的距离） */
		virtual void MouseRoller(int z) override;

		/** @brief 鼠标按键事件 */
		virtual void MouseButton(MouseBtn button, InputState State) override;

		/** @brief 键盘事件（自由飞行 + 快捷操作） */
		virtual void KeyDown(GameKeyEnum moveDirection) override;

		/** @brief 获取 CommandBuffer（把二级指令挂到主指令上） */
		virtual void GameCommandBuffers(unsigned int Format_i) override;

		/** @brief 游戏循环（拾取 → 物理步进 → 绘制 → 相机更新） */
		virtual void GameLoop(unsigned int mCurrentFrame) override;

		/** @brief 录制 CommandBuffer */
		virtual void GameRecordCommandBuffers() override;

		/** @brief 游戏停止界面循环 */
		virtual void GameStopInterfaceLoop(unsigned int mCurrentFrame) override;

		/** @brief 游戏 TCP 事件 */
		virtual void GameTCPLoop() override;

		/** @brief ImGui 游戏界面 */
		virtual void GameUI() override;

	private:
		// ═══════════════════════════════════════════════════════════════
		// 3D 拾取（取代 2D 的"射线打到 z=0 平面"）
		// ═══════════════════════════════════════════════════════════════

		/**
		 * @brief 从光标发出一条世界射线，步进命中测试
		 * @param outHit  [out] 命中点世界坐标；未命中时是射线与 y=0 平面的交点
		 * @param outObj  [out] 命中的物理对象（`PhysicsWorld3D::Get`）；未命中为 nullptr
		 * @return 是否命中"实体"（物体或地图体素）。false 表示 outHit 来自 y=0 回落
		 * @details
		 *  - 方向来自 `mCamera->screenToWorld(...)`（返回**归一化方向**）。
		 *  - 从 `mCamera->getCameraPos()` 出发，步长 `PickStep3D`，最多 `PickMaxStep3D` 步。
		 *  - 每步先问物体（`world->Get(point)`，内部网格粗筛 + 精确判定），
		 *    再问地图（取体素实体位；`MapFormwork3D::FMGetCollide(Vec3_)` 会自带
		 *    centrality 偏移，所以必须用浮点重载，否则世界坐标为负时失效）。
		 *  - 走到尽头都没命中 → 与地面 `y = 0` 求交（`t = -origin.y / dir.y`，
		 *    `dir.y >= 0` 或 `t < 0` 时视为无穷远，退化为"相机前方 6 单位"的落点）。
		 */
		bool PickWorldPoint3D(glm::vec3 &outHit, PhysicsBlock3D::PhysicsFormwork3D *&outObj);

		/**
		 * @brief 从当前视图矩阵反解自由飞行相机的 (Yaw, Pitch)
		 * @details Demo 切换会用 ApplyCameraView3D 直接摆位姿；不反解回成员，
		 *          下一次拖拽会把视角甩回旧值。
		 */
		void SyncCameraAnglesFromViewLocal();


		/**
		 * @brief 把"选中物体 + 当前 Z 层 + 拾取点"换算成体素坐标
		 * @param shape   目标形状（非空）
		 * @param worldPt 世界坐标拾取点
		 * @param outCell [out] 局部体素坐标
		 * @return 是否在形状网格范围内
		 * @details 形状 `pos` 是**质心**的世界坐标，局部网格原点的世界坐标是
		 *          `pos - QuatRotate(rotation, CentreMass)`；把 `worldPt` 减去原点后
		 *          乘 `inverse(mat3_cast(rotation))` 即得局部坐标（3D 版新增：
		 *          2D 只有标量角度，直接旋转即可）。
		 */
		bool WorldToShapeVoxel3D(PhysicsBlock3D::PhysicsShape3D *shape, glm::vec3 worldPt,
		                         glm::ivec3 &outCell);

		// ═══════════════════════════════════════════════════════════════
		// 相机（照 BlockWorld 的自由飞行模式）
		// ═══════════════════════════════════════════════════════════════

		/** @brief 依据 mYaw/mPitch/mCameraPos 重算朝向并写回 mCamera */
		void UpdateCamera();

		/** @brief 按当前朝向沿局部轴平移相机（自由飞行） */
		void MoveCameraLocal(glm::vec3 localDir);

		/** @brief 把 3D 引擎的 CPU 计时快照写进日志（用于对拍 2D 的 CPU 指标） */
		void LogCPUTimeProfile();

		// ═══════════════════════════════════════════════════════════════
		// 交互（2D 同名函数的 3D 改写）
		// ═══════════════════════════════════════════════════════════════

		/**
		 * @brief 编辑模式：左键创建物体 / 拖动选中物体 / 删除
		 * @param worldPos 光标拾取到的世界点
		 * @param haveRay  该点是否来自**真实射线命中**（false = 只是地面回落点）
		 * @note 2D 对应 `EditorMode(glm::vec2 huoqdedian)`；3D 多带一个 haveRay，
		 *       避免对着空处点击时把物体创建到无穷远的地面交点上。
		 */
		void EditorMode(glm::vec3 worldPos, bool haveRay);

		/** @brief 左键拖拽相机（桌面端：拖拽旋转视角，对应 2D 的 CameraLeftEvent） */
		void CameraLeftEvent(bool Click, bool First, glm::dvec2 s, glm::dvec2 e);

		/** @brief 右键拖拽相机 */
		void CameraRightEvent(bool Click, bool First, glm::dvec2 s, glm::dvec2 e);

		/** @brief 拖拽 `PhysicsAngle3D` 派生对象（形状 / 球 / 线段共用） */
		void SquareLeftEvent(PhysicsBlock3D::PhysicsAngle3D *Ptr, bool Click, bool First,
		                     glm::dvec3 s, glm::dvec3 e);

		/** @brief 拖拽粒子的辅助（用弹簧把粒子拉向光标） */
		void ParticleLeftEvent(PhysicsBlock3D::PhysicsParticle3D *Ptr, bool Click, bool First,
		                       glm::dvec3 s, glm::dvec3 e);

		/** @brief 在拾取点创建物体（2D 的 FoundLeftEvent） */
		void FoundLeftEvent(bool Click, bool First, glm::dvec3 s, glm::dvec3 e);

		/** @brief 右键菜单 */
		void RenderRightClickMenu();

		// ═══════════════════════════════════════════════════════════════
		// 体素编辑（3D 简化版）
		// ═══════════════════════════════════════════════════════════════

		/** @brief 进入体素编辑模式 */
		void EnterGridEdit(PhysicsBlock3D::PhysicsShape3D *shape);

		/** @brief 退出体素编辑模式 */
		void ExitGridEdit();

		/** @brief 体素编辑面板（ImGui） */
		void RenderGridEditUI();

		/**
		 * @brief 在当前 Z 层用笔刷涂抹体素
		 * @param shape  目标形状
		 * @param worldPt 光标拾取到的世界点
		 * @param erase   true = 擦除，false = 按笔刷设置绘制
		 */
		void GridEditPaint(PhysicsBlock3D::PhysicsShape3D *shape, glm::vec3 worldPt, bool erase);

		/**
		 * @brief 在 Begin()/End() 之间绘制体素编辑的三维提示
		 *        （当前层高亮框 + 笔刷落点框）
		 */
		void RenderGridEditPreview3D();

		/** @brief 切换 demo（销毁旧世界、执行 DemoFunS3D、重建地图轮廓） */
		void SwitchDemo3D(int index);

		/** @brief 重建地图轮廓静态缓冲（在 av->Begin() 之前调用） */
		void RefreshMapOutline();

	private:
		/** @brief 物理世界开关（关掉后不步进，但仍绘制） */
		bool PhysicsSwitch = true;

		/** @brief 物理世界（3D） */
		PhysicsBlock3D::PhysicsWorld3D *mPhysicsWorld = nullptr;

		/**
		 * @brief 当前地图（3D）
		 * @note 只是 `mPhysicsWorld->GetMapFormwork()` 的缓存，**不拥有**所有权；
		 *       世界析构时地图由世界释放（`SetMapFormwork` 语义与 2D 一致）。
		 *       故本类析构不 delete 它，避免二次释放。
		 */
		PhysicsBlock3D::MapFormwork3D *mMapFormwork = nullptr;

		/** @brief 当前动态地图（若 demo 用的是 MapDynamic3D，否则 nullptr） */
		PhysicsBlock3D::MapDynamic3D *mDynamicMap3D = nullptr;

		/** @brief 鼠标左键是否按下 */
		bool mLeftMouseDown = false;

		/** @brief 鼠标右键是否按下 */
		bool mRightMouseDown = false;

		/** @brief 缓存的窗口逻辑宽（用于射线与 UI 命中判断） */
		int mWinWidth = 0;

		/** @brief 缓存的窗口逻辑高 */
		int mWinHeight = 0;

		/** @brief 日志是否自动滚动到最新 */
		bool AutoScrollLog = true;

		/** @brief 当前选中的物理对象（多态基类指针，不拥有所有权） */
		PhysicsBlock3D::PhysicsFormwork3D *PhysicsFormworkPtr = nullptr;

		/** @brief 是否显示物理辅助信息（位置/速度/受力/碰撞点等） */
		bool PhysicsAssistantInformation = false;

		/** @brief 是否显示地图轮廓 */
		bool ShowMapOutline = true;

		// ── 渲染方式（线 / 面）────────────────────────────────────────
		/**
		 * @brief 渲染方式状态：全局默认 + 地形单独 + 单物体覆盖
		 * @details 见 `PhysicsBlock3D/RenderMode3D.hpp`。线框由
		 *          `AuxiliaryVision` 的 Line 管线绘制；"面"由
		 *          `PhysicsFaceRenderer3D`（BlockWorld 同款三角形管线）绘制。
		 *          判定优先级：单物体覆盖 > 地形单独 > 全局默认。
		 */
		PhysicsBlock3D::RenderModeState3D mRenderModes;
		/** @brief 「面」渲染器；`Init()` 失败时为 nullptr（退化为纯线框） */
		PhysicsFaceRenderer3D *mFaceRenderer = nullptr;

		// ── 相机状态（照 BlockWorld）───────────────────────────────────
		/** @brief 相机世界位置 */
		glm::vec3 mCameraPos{40.0f, 30.0f, 60.0f};
		/** @brief 偏航角（度） */
		float mYaw = -135.0f;
		/** @brief 俯仰角（度，负值向下看） */
		float mPitch = -25.0f;
		/** @brief 自由飞行速度（单位/秒） */
		float mCameraSpeed = 20.0f;
		/** @brief 鼠标灵敏度 */
		float mCameraSensitivity = 0.15f;
		/** @brief 上一次鼠标位置（拖拽增量用） */
		double mLastMouseX = 0.0;
		double mLastMouseY = 0.0;
		/** @brief 是否是进入窗口后的第一次移动（丢弃首帧跳变） */
		bool mFirstMouseMove = true;
		/** @brief 本帧相机累积的平移输入（由 KeyDown 累加，UpdateCamera 消费后清零） */
		glm::vec3 mCameraMoveInput{0.0f};

		// ── 物理步进 ──────────────────────────────────────────────────
		/** @brief 固定物理步长（1/100 秒，与引擎九阶段自洽，照 2D） */
		static constexpr double PhysicsTick = 1.0 / 100.0;
		/** @brief 步长累加器 */
		double PhysicsAccumulator = 0.0;
		/** @brief 上一帧的 CPU 侧耗时（引擎公开计时之和，ms），取代 2D 的 mCPULastFrameTime */
		float mCPUTimeMS = 0.0f;

		// ── 拾取 ──────────────────────────────────────────────────────
		/** @brief 本帧光标命中的世界点 */
		glm::vec3 mPickHit{0.0f};
		/** @brief 本帧是否真实命中（物体或地图），false 时 mPickHit 是 y=0 回落点 */
		bool mPickHitValid = false;
		/** @brief 本帧命中的对象（nullptr = 命中地图或都没命中） */
		PhysicsBlock3D::PhysicsFormwork3D *mPickObject = nullptr;
		/** @brief 创建物体时使用的落点（拾取点，或相机前方 mSpawnDistance 处） */
		glm::vec3 mSpawnPoint{0.0f};
		/** @brief 相机前方无命中时的创建距离（滚轮可调） */
		float mSpawnDistance = 12.0f;
		/** @brief 拾取步进步长 */
		static constexpr float PickStep3D = 0.25f;
		/** @brief 拾取最大步数（0.25 × 400 = 100 单位） */
		static constexpr int PickMaxStep3D = 400;

		// ── 编辑 / 创建 ────────────────────────────────────────────────
		/** @brief 是否处于刚体编辑模式（左键创建/拖动/删除） */
		bool EditorModeBool = false;
		/** @brief 待创建物体的类型索引 */
		int item_object = 0;
		/** @brief 待创建物体的类型名 */
		const char *objectName[4] = {u8"圆", u8"点", u8"网格", u8"线"};

		/** @brief 创建球用的半径 */
		float CreateRadius = 1.0f;
		/** @brief 创建物体的质量 */
		float CreateMass = 1.0f;
		/** @brief 创建物体的摩擦因数 */
		float CreateFriction = 0.2f;
		/** @brief 创建网格形状的尺寸（体素） */
		glm::ivec3 CreateShapeSize{4, 4, 4};
		/** @brief 创建线段用的半长度（线段总长 = 2 × 该值） */
		float CreateLineHalfLength = 3.0f;
		/** @brief 创建线段的胶囊厚度 */
		float CreateLineThickness = 0.2f;
		/** @brief 上一个创建出来的对象（用于连续创建时错开位置） */
		PhysicsBlock3D::PhysicsFormwork3D *mLastCreated = nullptr;

		// ── 右键菜单 ──────────────────────────────────────────────────
		/** @brief 右键点击到的对象 */
		PhysicsBlock3D::PhysicsFormwork3D *RightClickObjectPtr = nullptr;
		/** @brief 是否显示右键菜单 */
		bool ShowRightClickMenu = false;

		// ── Demo 控制 ─────────────────────────────────────────────────
		/** @brief 当前 demo 索引 */
		int mCurrentDemo = 0;
		/** @brief 世界重力加速度（UI 可直接改，creates 新世界时用） */
		glm::vec3 mGravity{0.0f, -9.8f, 0.0f};
		/** @brief 是否启用风力（部分 demo 会开） */
		bool mWindBool = false;
		/** @brief 风力 */
		glm::vec3 mWind{0.0f};

		// ── 体素编辑（3D 简化版）───────────────────────────────────────
		/** @brief 编辑目标形状 */
		PhysicsBlock3D::PhysicsShape3D *GridEditShape = nullptr;
		/** @brief 编辑目标的 Z 层（滑块选层） */
		int GridEditLayer = 0;
		/** @brief 是否保存进入编辑时的物理开关状态，退出时还原 */
		bool GridEditSavedPhysicsSwitch = true;
		/** @brief 笔刷类型 */
		enum class GridEditBrush3D
		{
			Draw,
			Erase
		} GridEditBrushType = GridEditBrush3D::Draw;
		/** @brief 笔刷写入的碰撞位 */
		bool GridEditCollision = true;
		/** @brief 笔刷写入的实体位 */
		bool GridEditEntity = true;
		/** @brief 笔刷写入的碰撞事件位 */
		bool GridEditEvent = false;
		/** @brief 笔刷写入的质量 */
		float GridEditMass = 1.0f;
		/** @brief 笔刷写入的摩擦 */
		float GridEditFriction = 0.2f;
		/** @brief 笔刷是否写入质量（关闭则只改位标志，保留原质量） */
		bool GridEditWriteMass = true;
		/** @brief 本帧笔刷落点的体素坐标（世界中，仅用于预览） */
		glm::ivec3 GridEditHoverCell{-1, -1, -1};
		/** @brief 本帧笔刷是否落点有效 */
		bool GridEditHoverValid = false;
		/** @brief 编辑模式下是否允许笔刷触碰之外也显示层框 */
		bool GridEditShowLayerBox = true;

		// ── 拖拽状态 ──────────────────────────────────────────────────
		/** @brief 左键拖拽的起点（世界坐标） */
		glm::dvec3 DragStart{0.0};
		/** @brief 左键是否处于"拖拽中" */
		bool Dragging = false;
		/** @brief 右键拖拽是否处于自旋中 */
		bool RightDragging = false;
		/** @brief 右键拖拽的起点（世界坐标） */
		glm::dvec3 RightDragStart{0.0};
		/** @brief 被拖拽对象的局部锚点（世界系抓取点 - 物体质心，在物体局部坐标下） */
		glm::dvec3 DragLocalAnchor{0.0};
		/** @brief 当前被拖拽的物理对象 */
		PhysicsBlock3D::PhysicsFormwork3D *DragObject = nullptr;
	};

}
