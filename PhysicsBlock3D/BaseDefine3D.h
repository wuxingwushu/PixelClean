#pragma once
#include <cmath>

// ═══════════════════════════════════════════════════════════════════════════
// PhysicsBlock3D —— 3D 物理引擎编译期定义
// 由 PixelClean/PhysicsBlock/BaseDefine.h 逐项改写而来。
// 所有宏均带 3D 后缀，以便与 2D PhysicsBlock 在同一工程内共存而不冲突。
// ═══════════════════════════════════════════════════════════════════════════

// 转动惯量采样数量（每轴采样数，3D 总采样 = N^3）
#define MomentInertiaSamplingSize3D 5

// 像素块（体素块）边长，必须是 2 的幂
#define PixelBlockEdgeSize3D 16
#define PixelBlockPowerMaxNum3D 4   // 16 是 2 的 4 次幂
#define PixelBlockPowerMinNum3D 0xF // 4 位 bit

// Z 轴分块边长（3D 新增：允许 X/Y/Z 不同分块尺寸，缺省与 XY 相同）
#define PixelBlockEdgeSizeZ3D 16
#define PixelBlockPowerMaxNumZ3D 4
#define PixelBlockPowerMinNumZ3D 0xF

#define Define_MinSpoilageBool3D 1
#if Define_MinSpoilageBool3D
#define Define_MinSpoilage3D 0.9994 // 能量转换率（60Hz 帧频下每帧的速度保留系数）

namespace PhysicsBlock3D
{
    /**
     * @brief 按时间计算阻尼保留系数
     * @param time 时间差（秒）
     * @return 该时间步内的速度保留系数（与帧率无关）
     * @details 与 2D 版语义一致：Define_MinSpoilage3D 是 60Hz 下"每帧"的
     *          速度保留系数，换算为按时间连续衰减 factor = k^(time)。
     */
    template<typename T>
    inline T MinSpoilageFactor(T time)
    {
        static const double kPerSecond = std::pow(static_cast<double>(Define_MinSpoilage3D), 60.0);
        return static_cast<T>(std::pow(kPerSecond, static_cast<double>(time)));
    }
}
#endif

#define PhysicsContactMaxSize3D 20 // 每对物体之间的最大碰撞点数量

// 3D 流形上限：face-clipped 流形最多 4 个接触点（比 2D 的 20 点遍历截断更紧）
#define PhysicsContactManifoldMaxSize3D 4

#define PhysicsApplyImpulseSize3D 10 // 最低物理迭代次数

#define PhysicsSleepThreshold3D 10 // 物体休眠阈值（StaticNum 超过此值视为静止）

// 液体子系统总开关（PhysicsLiquid3D 是 3D PBF 求解器，已落地并通过行为验证）
// 置 1：PhysicsWorld3D 暴露 SetLiquid/GetLiquid，并在后处理阶段自动调用 Update；
// 置 0：液体类仍可单独使用（自行驱动 Update），但世界不会自动更新它。
// 出厂默认 0；两种取值均已通过编译验证：
//   0：PhysicsWorld3D.cpp 单独编译一次通过（液体接口不参与）；
//   1：另用 scratch 构建树编译，SetLiquid / mLiquid->Update / NotifyParticleRemoved 三处调用点全部类型检查通过。
#define PhysicsLiquid3DBool3D 1

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ───────────────────────────────────────────────────────────────────────────
// 编译期功能开关
// ───────────────────────────────────────────────────────────────────────────
//
// 【PhysicsGPU 明确排除】
// 2D 版的 PhysicsGPU 是 Vulkan 计算实现（外置 SPIR-V：physics_arbiter.comp /
// physics_joint.comp / physics_junction.comp），其 Body 缓冲硬编码
// BODY_STRIDE=8 / cross2D / vec2 tangent，属强 2D 绑定。
// 3D 移植不包含该模块：本目录没有 PhysicsGPU3D.*，也没有 PhysicsGPUBool 开关。
// PhysicsWorld3D 的冲量迭代一律走 CPU（多线程）路径。
// 若将来需要 GPU 加速，须重写三个着色器为 vec3/cross3 + 3×3 逆惯量张量，
// 并把 BODY_FLOATS 从 8 扩到约 18~21。
