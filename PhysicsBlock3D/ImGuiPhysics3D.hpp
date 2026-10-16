#pragma once
#include <string>  // Dvec3ImGui3D/GridImGui3D 形参
#include <vector>  // AuxiliaryReadBoolColor3D / AuxiliaryStorageBoolColor3D 宏体
#include "PhysicsParticle3D.hpp"
#include "PhysicsShape3D.hpp"
#include "PhysicsCircle3D.hpp"
#include "PhysicsLine3D.hpp"
#include "PhysicsWorld3D.hpp"

/*
 * ImGui / ini 头文件路径切换（TranslatorLocality3D）：
 *   本地开发时走 ../ImGui/imgui.h + ../ini.h，随引擎一起分发时走
 *   ../extern/imgui/imgui.h + ini.h。
 * 说明：2D 版原先把这段切换写在 ImGuiPhysics.cpp 里（不是 .hpp），
 *       这里按要求同时在 .hpp 与 .cpp 保留一份 —— 两侧路径都能解析，
 *       重复包含由 imgui 自身的 #pragma once / 守卫消化，无副作用。
 * TranslatorLocality3D 由上面 PhysicsWorld3D.hpp → BaseStruct3D.hpp 定义，
 * 因此这段必须放在引擎头文件之后。
 */
#if TranslatorLocality3D
#include "../ImGui/imgui.h"
#include "../ini.h"
#else
#include "../extern/imgui/imgui.h"
#include "ini.h"
#endif

/*
 * ═══════════════════════════════════════════════════════════════════════════
 * ImGuiPhysics3D —— 由 PhysicsBlock/ImGuiPhysics.hpp + .cpp 移植
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * 【刻意删除的内容（3D 无对应物）】
 *   · `bool PhysicsUI(PhysicsLiquid *)` 重载 —— 整个液体子系统在 3D 中
 *     不存在：PhysicsLiquid3DBool3D（BaseDefine3D.h:56）为 0，且目录里
 *     没有 PhysicsLiquid3D.hpp/.cpp。故连同 `#include "PhysicsLiquid*.hpp"`
 *     一并删除。将来液体落地后需要补回这个重载。
 *
 * 【命名约定】
 *   2D 的 `Auxiliary_*Bool` / `Auxiliary_*Color` 定义在**全局作用域**
 *   （PhysiBlock/ImGuiPhysics.cpp 里没有 namespace 包裹）。若 3D 沿用同名，
 *   同一工程内与 2D 版一起链接就会符号重定义，因此本版给全局变量统一加
 *   `3D` 后缀（Auxiliary_PosBool3D 等），宏也同样加后缀避免预处理期冲突。
 *   函数本体放在 namespace PhysicsBlock3D 内，天然与 2D 版隔离。
 */

// 辅助声明变量（3D 版：变量名加 3D 后缀，避免与 2D 版全局符号冲突）
#define AuxiliaryExternBoolColor3D(Name)  \
	extern bool Auxiliary_##Name##Bool3D; \
	extern float Auxiliary_##Name##Color3D[4];

// 名字字符串化（3D 版）
#define TOSIRING3D(str) #str

// 辅助实现变量（3D 版）
#define AuxiliaryBoolColor3D(Name)           \
	bool Auxiliary_##Name##Bool3D = false;   \
	float Auxiliary_##Name##Color3D[4] = {0, 0, 0, 0};

// 辅助读取信息（3D 版）
// 三个 Get 都带默认值：ini 里可能没有这个键（上一版遗留的文件、被手工删改过），
// 而 ini.h 里不带默认值的 Get 遇到缺键会抛 std::runtime_error（ini.h:362），
// 抛出后会中断 AuxiliaryInfoRead 中后续所有键的读取。带默认值 = 读到哪算哪。
#define AuxiliaryReadBoolColor3D(Name)                                                        \
	Auxiliary_##Name##Bool3D = Ini.Get<bool>("Auxiliary", TOSIRING3D(Name##Bool3D), false);   \
	std::vector<float> Name##Color3D =                                                        \
		Ini.GetVector<float>("Auxiliary", TOSIRING3D(Name##Color3D), {0, 0, 0, 0});            \
	for (size_t i = 0; i < Name##Color3D.size() && i < 4u; ++i)                               \
	{                                                                                         \
		Auxiliary_##Name##Color3D[i] = Name##Color3D[i];                                      \
	}

// 辅助保存信息（3D 版）
#define AuxiliaryStorageBoolColor3D(Name)                                                  \
	Ini.UpdateEntry("Auxiliary", TOSIRING3D(Name##Bool3D), Auxiliary_##Name##Bool3D);      \
	Ini.UpdateEntry("Auxiliary", TOSIRING3D(Name##Color3D),                                \
	                std::vector<float>(Auxiliary_##Name##Color3D, Auxiliary_##Name##Color3D + 4));

// 颜色数组 → glm::vec4（3D 版；2D 版名为 ColorToVec4）
#define ColorToVec4_3D(color) \
	glm::vec4 { color[0], color[1], color[2], color[3] }

namespace PhysicsBlock3D
{
	/**
	 * @brief 绘图用的默认标记半径
	 * @details 2D 版把每个辅助点都画成 0.05 的 spot；3D 下"点"是半径 0.05
	 *          的球状标记，语义与 2D 完全一致（都是固定世界尺度的小标记）。 */
	static const float AuxiliaryMarkSize3D = 0.05f;

	// 声明变量
	// 物理对象基础信息
	AuxiliaryExternBoolColor3D(Pos);		   // 位置
	AuxiliaryExternBoolColor3D(OldPos);		   // 旧位置
	AuxiliaryExternBoolColor3D(Angle);		   // 角度（3D 下画的是局部 X 朝向）
	AuxiliaryExternBoolColor3D(Speed);		   // 速度
	AuxiliaryExternBoolColor3D(Force);		   // 受力
	AuxiliaryExternBoolColor3D(CentreMass);	   // 质心
	AuxiliaryExternBoolColor3D(CentreShape);   // 几何中心
	AuxiliaryExternBoolColor3D(Outline);	   // 外骨骼点
	AuxiliaryExternBoolColor3D(MaxOutlineCentreMass); // 最大外骨骼点质心（3D 对应 SeedPos）

	// 碰撞信息
	AuxiliaryExternBoolColor3D(CollisionDrop);				  // 碰撞点
	AuxiliaryExternBoolColor3D(SeparateNormalVector);		  // 分离法向量
	AuxiliaryExternBoolColor3D(CollisionDropToCenterOfGravity); // 碰撞点到两个物体重心的连线
	AuxiliaryExternBoolColor3D(GridDivided);				  // 被选中的物体被划分网格到的网格位置

	// 读取物理辅助显示信息（.ini 文件名为 PhysicsBlock3D.ini，不复用 2D 的 PhysicsBlock.ini）
	void AuxiliaryInfoRead();

	// 保存物理辅助显示信息
	void AuxiliaryInfoStorage();

	// 物理辅助显示参数的控制UI
	void PhysicsAuxiliaryColorUI();

	/**
	 * @brief 将 PhysicsLine3D 变量UI显示
	 * @param Object PhysicsLine3D 指针
	 * @return 是否修改导致需要更新对象信息 */
	bool PhysicsUI(PhysicsLine3D *Object);

	/**
	 * @brief 将 PhysicsCircle3D 变量UI显示
	 * @param Object PhysicsCircle3D 指针
	 * @return 是否修改导致需要更新对象信息 */
	bool PhysicsUI(PhysicsCircle3D *Object);

	/**
	 * @brief 将 PhysicsShape3D 变量UI显示
	 * @param Object PhysicsShape3D 指针
	 * @return 是否修改导致需要更新对象信息 */
	bool PhysicsUI(PhysicsShape3D *Object);

	/**
	 * @brief 将 PhysicsAngle3D 变量UI显示
	 * @param Object PhysicsAngle3D 指针
	 * @return 是否修改导致需要更新对象信息 */
	bool PhysicsUI(PhysicsAngle3D *Object);

	/**
	 * @brief 将 PhysicsParticle3D 变量UI显示
	 * @param Object PhysicsParticle3D 指针
	 * @return 是否修改导致需要更新对象信息 */
	bool PhysicsUI(PhysicsParticle3D *Object);

	/**
	 * @brief 将 PhysicsWorld3D 变量UI显示
	 * @param Object PhysicsWorld3D 指针
	 * @return 是否修改导致需要更新对象信息 */
	bool PhysicsUI(PhysicsWorld3D *Object);

	// ─────────────────────────────────────────────────────────────
	// 内部辅助（3D 版；同样加 3D 后缀避免与 2D 版链接冲突）
	// ─────────────────────────────────────────────────────────────

	/**
	 * @brief 三分量拖拽控件（2D 版 Dvec2ImGui 的 3D 对应物） */
	void Dvec3ImGui3D(std::string name, Vec3_ *val);

	/**
	 * @brief 单个体素的调试编辑控件（2D 版 GridImGui 的 3D 对应物）
	 * @param name 显示名前缀
	 * @param Grid 体素引用（GridBlock3D）
	 * @return 关键字段是否被修改（调用方据此决定是否重建物体信息） */
	bool GridImGui3D(std::string name, GridBlock3D *Grid);

}
