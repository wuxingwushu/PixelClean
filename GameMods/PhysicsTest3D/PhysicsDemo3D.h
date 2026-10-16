#pragma once
#include "../../PhysicsBlock3D/PhysicsWorld3D.hpp"
#include "../../PhysicsBlock3D/MapStatic3D.hpp"
#include "../../PhysicsBlock3D/PhysicsLiquid3D.hpp"
#include "../../Camera/Camera.h"

/*
 * ═══════════════════════════════════════════════════════════════════════════
 * PhysicsDemo3D —— 由 GameMods/PhysicsTest/PhysicsDemo.h + .cpp 移植
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * 与 2D 版的三个必须注意的差异：
 *   1. 每个 Demo 函数负责**销毁旧世界并新建一个**、把摄像机摆到合适位置，
 *      然后搭建场景。这一点与 2D 完全相同。
 *   2. 3D 没有"画面平面"（2D 把一切压在 z=0）：摄像机是**自由飞行**的，
 *      所以每个 Demo 必须自己给出一组 `(位置, yaw, pitch)`。
 *      本头文件的 `CameraView3D` 结构体就是为此提供的载体 —— 2D 版只有
 *      `mCamera->setCameraPos({x, y, 50})` 一行，3D 需要三个量。
 *   3. 2D 的 `Vec2_` 全部变 `Vec3_`；`PhysicsShape` 的 `{w, h}` 变 `{w, h, d}`；
 *      `PhysicsWorld({0,-9.8}, false)` 变 `PhysicsWorld3D(Vec3_(0,-9.8f,0), false)`。
 *
 * 命名空间仍是 `PhysicsBlock3D`（不是 GAME），与 2D 版结构对应。
 */

namespace PhysicsBlock3D
{
	/**
	 * @brief 一个 Demo 给摄像机的初始视角
	 * @details 2D 版把摄像机放在 z=50 俯视 z=0 平面，只需要一个位置；
	 *          3D 版必须同时给出朝向，故用本结构体携带。
	 *          `Yaw/Pitch` 语义与 `GameMods/BlockWorld` 的自由飞行相机一致：
	 *          `front = (cos(pitch)·sin(yaw), sin(pitch), cos(pitch)·cos(yaw))`，
	 *          单位为**度**。`Pitch = -90` 表示垂直向下看。
	 */
	struct CameraView3D
	{
		Vec3_ Pos{0.0f, 30.0f, 40.0f}; ///< 摄像机位置
		FLOAT_ Yaw = 0.0f;			   ///< 偏航（度）
		FLOAT_ Pitch = -25.0f;		   ///< 俯仰（度，负值向下看）
	};

	/// 便捷：把 CameraView3D 应用到 Camera（设置位置 + lookAt + update）
	void ApplyCameraView3D(Camera *mCamera, const CameraView3D &view);

	/// 便捷：把一个 Demo 的场景包围盒中心 + 半径转成合适的观察视角
	CameraView3D MakeOrbitView3D(Vec3_ center, FLOAT_ radius);

	// ─────────────────────────────────────────────────────────────
	// 场景构建工具（各 Demo 共用；放在 .cpp 的匿名命名空间会重复定义，
	// 故统一由 PhysicsDemo3D.cpp 提供，这里只声明）
	// ─────────────────────────────────────────────────────────────

	/**
	 * @brief 建一个 `size³` 的静态地图：底面两层为地板，四周一圈围墙
	 * @param size   边长（体素）
	 * @param height 围墙高度
	 * @return 已设置好 Entity/Collision 与 Centrality 的 MapStatic3D（调用方负责 SetMapFormwork）
	 */
	MapStatic3D *BuildBoxMap3D(int size, int height);

	/// 建一个纯地板地图（无边墙），用于"物体滑出地图"的可视化
	MapStatic3D *BuildFloorMap3D(int size);

	/// 在一个 PhysicsShape3D 上把全部体素设为实体+碰撞+指定质量，并 UpdateAll()
	PhysicsShape3D *MakeSolidShape3D(Vec3_ pos, int w, int h, int d, FLOAT_ mass = 1.0f, FLOAT_ friction = 0.2f);

	// ─────────────────────────────────────────────────────────────
	// Demo 场景（与 2D 版 24 个 Demo 一一对应，逐条改写为 3D）
	// ─────────────────────────────────────────────────────────────

	/// 0 外骨骼计算（用关节把手脚连到躯干）
	void PhysicsDemo3D0(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 1 几何重心（不同质量的方块自由落体后停在何处）
	void PhysicsDemo3D1(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 2 方块堆叠
	void PhysicsDemo3D2(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 3 方块堆叠金字塔
	void PhysicsDemo3D3(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 4 点和方块
	void PhysicsDemo3D4(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 5 关节
	void PhysicsDemo3D5(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 6 铁链
	void PhysicsDemo3D6(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 7 杠杆连接
	void PhysicsDemo3D7(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 8 橡皮筋受力
	void PhysicsDemo3D8(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 9 绳子与地形碰撞
	void PhysicsDemo3D9(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 10 球
	void PhysicsDemo3D10(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 11 球压力测试（4×4×4=64 球阵列）
	void PhysicsDemo3D11(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 12 绳子牛顿摆
	void PhysicsDemo3D12(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 13 关节牛顿摆
	void PhysicsDemo3D13(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 14 摩擦力（不同摩擦因数的斜坡）
	void PhysicsDemo3D14(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 15 动态网格搜索测试（大量物体逼迫格网频繁重建）
	void PhysicsDemo3D15(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 16 物理线段（胶囊）
	void PhysicsDemo3D16(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 17 线段绳子
	void PhysicsDemo3D17(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 18 组合测试（形状/球/点/线段 + 关节 + 连接 + 组装体同场）
	void PhysicsDemo3D18(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 19 碰撞回调系统
	void PhysicsDemo3D19(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 20 运动学物体
	void PhysicsDemo3D20(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 21 触发器系统
	void PhysicsDemo3D21(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 22 动态地图（无限生成）
	void PhysicsDemo3D22(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	/// 23 液体模拟（PBF）
	void PhysicsDemo3D23(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	using DemoFunction3D = void (*)(PhysicsWorld3D **myPhysicsWorld, Camera *mCamera);

	static DemoFunction3D DemoFunS3D[] = {
		PhysicsDemo3D0,
		PhysicsDemo3D1,
		PhysicsDemo3D2,
		PhysicsDemo3D3,
		PhysicsDemo3D4,
		PhysicsDemo3D5,
		PhysicsDemo3D6,
		PhysicsDemo3D7,
		PhysicsDemo3D8,
		PhysicsDemo3D9,
		PhysicsDemo3D10,
		PhysicsDemo3D11,
		PhysicsDemo3D12,
		PhysicsDemo3D13,
		PhysicsDemo3D14,
		PhysicsDemo3D15,
		PhysicsDemo3D16,
		PhysicsDemo3D17,
		PhysicsDemo3D18,
		PhysicsDemo3D19,
		PhysicsDemo3D20,
		PhysicsDemo3D21,
		PhysicsDemo3D22,
		PhysicsDemo3D23};

	static const char *DemoNameS3D[] = {
		"外骨骼计算",
		"几何重心",
		"方块堆叠",
		"方块堆叠金字塔",
		"点和方块",
		"关节",
		"铁链",
		"杠杆连接",
		"橡皮筋受力",
		"绳子与地形碰撞",
		"球",
		"球压力测试",
		"绳子牛顿摆",
		"关节牛顿摆",
		"摩擦力",
		"动态网格搜索测试",
		"物理线段",
		"线段绳子",
		"组合测试",
		"碰撞回调系统",
		"运动学物体",
		"触发器系统",
		"动态地图（无限生成）",
		"液体模拟"};

	/// Demo 总数（供 UI 循环使用，避免各处写死 24）
	static const int DemoCount3D = (int)(sizeof(DemoFunS3D) / sizeof(DemoFunS3D[0]));
}
