#pragma once
#include <cmath>

#define MomentInertiaSamplingSize 5 // 转动惯量采样数量

#define PixelBlockEdgeSize 16     // 像素块边长， （必须是 2 的幂，方便计算）
#define PixelBlockPowerMaxNum 4   // 16 是 2 的 4次幂
#define PixelBlockPowerMinNum 0xF // 4 位bit

#define Define_MinSpoilageBool 1 // 能量转换率 是否开关
#if Define_MinSpoilageBool
#define Define_MinSpoilage 0.9994 // 能量转换率（60Hz 帧频下每帧的速度保留系数）

namespace PhysicsBlock
{
    /**
     * @brief 按时间计算阻尼保留系数
     * @param time 时间差（秒）
     * @return 该时间步内的速度保留系数（与帧率无关）
     * @details 原 Define_MinSpoilage 是 60Hz 下"每帧"的速度保留系数，
     *          按帧直接相乘会随帧率变化（60Hz 每秒剩 0.9994^60≈0.965，
     *          144Hz 每秒仅剩 0.9994^144≈0.917）。
     *          此处换算为按时间连续衰减：factor = Define_MinSpoilage^(time * 60)。
     *          为保证 60Hz 下的行为与旧实现一致，以 60Hz 作为换算基准。 */
    template<typename T>
    inline T MinSpoilageFactor(T time)
    {
        static const double kPerSecond = std::pow(static_cast<double>(Define_MinSpoilage), 60.0);
        return static_cast<T>(std::pow(kPerSecond, static_cast<double>(time)));
    }
}
#endif

#define PhysicsContactMaxSize 20 // 每对物体之间的最大碰撞点数量（过多会增加计算负担）

#define PhysicsApplyImpulseSize 10 // 最低物理迭代次数

#define PhysicsSleepThreshold 10 // 物体休眠阈值（StaticNum超过此值视为静止）

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
