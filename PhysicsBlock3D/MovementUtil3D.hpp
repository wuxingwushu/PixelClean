#pragma once
#include "BaseDefine3D.h"
#include "BaseStruct3D.hpp"
#include <cmath>

namespace PhysicsBlock3D
{
    // =========================================================================
    // 移动驱动辅助工具（方案E混合驱动配套）—— 3D 版
    // 由上层 MovementComponent 调用，把"目标速度"等抽象意图换算成具体的力，
    // 走 AddForce → 物理引擎牛顿积分路径（自动尊重 mass / kinematic 状态）。
    // =========================================================================

    /**
     * @brief 由"目标速度"反推本帧应施加的力（含阻尼补偿）
     * @param currentSpeed 当前速度
     * @param desiredSpeed 期望的下一帧速度
     * @param mass 物体质量（<=0 表示不可动，返回 0）
     * @param dt 本帧时间步长
     * @return 应施加的力（直接传给 AddForce）
     *
     * @details 推导自 PhysicsParticle3D::PhysicsSpeed：
     *          speed_next = damp * speed_cur + dt * (Ga + invMass * force)
     *          忽略重力（Ga 由 PhysicsWorld3D 统一施加），令 speed_next = desiredSpeed：
     *          force = mass * (desiredSpeed - damp * currentSpeed) / dt
     *          其中 damp = Define_MinSpoilage3D（开阻尼时 0.9994，否则 1.0）。
     *          这样物体能在若干帧内平滑趋近目标速度，且稳态误差为 0。
     *          3D 只是把逐分量扩展为三分量，数学完全一致。 */
    inline Vec3_ ComputeMoveForce(const Vec3_ &currentSpeed, const Vec3_ &desiredSpeed,
                                  FLOAT_ mass, FLOAT_ dt)
    {
        if (mass <= 0 || dt <= 0)
        {
            return Vec3_(0.0f);
        }
#if Define_MinSpoilageBool3D
        const FLOAT_ damp = Define_MinSpoilage3D;
#else
        const FLOAT_ damp = FLOAT_(1);
#endif
        const FLOAT_ invDt = FLOAT_(1) / dt;
        return Vec3_((desiredSpeed.x - damp * currentSpeed.x) * mass * invDt,
                     (desiredSpeed.y - damp * currentSpeed.y) * mass * invDt,
                     (desiredSpeed.z - damp * currentSpeed.z) * mass * invDt);
    }

    /**
     * @brief 帧率无关的指数平滑系数
     * @param rate 趋近率（越大越快；例如 10 表示约每秒衰减到 1/e 的距离）
     * @param dt 时间步长
     * @return lerp 系数 k∈[0,1]，用法：value = lerp(value, target, k)
     *
     * @details k = 1 - exp(-rate * dt)，保证不同帧率下趋近曲线一致。
     *          与维度无关，3D 原样保留。 */
    inline float SmoothK3D(float rate, float dt)
    {
        return 1.0f - std::exp(-rate * dt);
    }

    /**
     * @brief 朝向球面插值（3D 版，取代 2D 的 LerpAngle）
     * @param current 当前朝向四元数
     * @param target 目标朝向四元数
     * @param k 平滑系数（来自 SmoothK3D）
     * @return 插值并归一化后的朝向
     *
     * @details 2D 的 LerpAngle 把角度差值规整到 (-π, π] 以保证走最短弧；
     *          3D 用 glm::slerp 天然走最短弧且角速度均匀，
     *          线性插值四元数会非匀速并可能绕远路。
     *          这是 3D 相对 2D 必须替换的一处数学。 */
    inline Quat_ LerpRotation3D(const Quat_ &current, const Quat_ &target, float k)
    {
        return glm::normalize(glm::slerp(current, target, k));
    }

} // namespace PhysicsBlock3D
