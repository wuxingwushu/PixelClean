#pragma once
#define MomentInertiaSamplingSize 5 // 转动惯量采样数量

#define PixelBlockEdgeSize 16     // 像素块边长， （必须是 2 的幂，方便计算）
#define PixelBlockPowerMaxNum 4   // 16 是 2 的 4次幂
#define PixelBlockPowerMinNum 0xF // 4 位bit

#define Define_MinSpoilageBool 1 // 能量转换率 是否开关
#if Define_MinSpoilageBool
#define Define_MinSpoilage 0.9994 // 能量转换率
#endif

#define PhysicsApplyImpulseSize 10 // 最低物理迭代次数

#define PhysicsSleepThreshold 10 // 物体休眠阈值（StaticNum超过此值视为静止）

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
