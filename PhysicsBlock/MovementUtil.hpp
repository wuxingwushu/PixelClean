#pragma once
#include "BaseDefine.h"
#include "BaseStruct.hpp"
#include <cmath>

namespace PhysicsBlock
{
    // =========================================================================
    // 移动驱动辅助工具（方案E混合驱动配套）
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
     * @details 推导自 PhysicsParticle::PhysicsSpeed：
     *          speed_next = damp * speed_cur + dt * (Ga + invMass * force)
     *          忽略重力（Ga 由 PhysicsWorld 统一施加），令 speed_next = desiredSpeed：
     *          force = mass * (desiredSpeed - damp * currentSpeed) / dt
     *          其中 damp = Define_MinSpoilage（开阻尼时 0.9994，否则 1.0）。
     *          这样物体能在若干帧内平滑趋近目标速度，且稳态误差为 0。 */
    inline Vec2_ ComputeMoveForce(const Vec2_& currentSpeed, const Vec2_& desiredSpeed,
                                  FLOAT_ mass, FLOAT_ dt)
    {
        if (mass <= 0 || dt <= 0)
        {
            return {0, 0};
        }
#if Define_MinSpoilageBool
        const FLOAT_ damp = Define_MinSpoilage;
#else
        const FLOAT_ damp = FLOAT_(1);
#endif
        const FLOAT_ invDt = FLOAT_(1) / dt;
        return Vec2_{ (desiredSpeed.x - damp * currentSpeed.x) * mass * invDt,
                      (desiredSpeed.y - damp * currentSpeed.y) * mass * invDt };
    }

    /**
     * @brief 帧率无关的指数平滑系数
     * @param rate 趋近率（越大越快；例如 10 表示约每秒衰减到 1/e 的距离）
     * @param dt 时间步长
     * @return lerp 系数 k∈[0,1]，用法：value = lerp(value, target, k)
     *
     * @details k = 1 - exp(-rate * dt)，保证不同帧率下趋近曲线一致。
     *          用于朝向平滑跟随、Ragdoll 模式恢复等场景。 */
    inline float SmoothK(float rate, float dt)
    {
        return 1.0f - std::exp(-rate * dt);
    }

    /**
     * @brief 角度环绕感知的线性插值（取最短弧）
     * @param current 当前角度（弧度）
     * @param target 目标角度（弧度）
     * @param k 平滑系数（来自 SmoothK）
     * @return 插值后的角度
     *
     * @details 把差值规整到 (-π, π]，保证朝向旋转走最短弧，不会出现"转 350° 到达 +10°"的情况。
     *          用于消除角色朝向瞬切（angle 直接赋值）的突兀感。 */
    inline float LerpAngle(float current, float target, float k)
    {
        constexpr float TWO_PI = 6.28318530717958647692f;
        constexpr float PI = 3.14159265358979323846f;
        float diff = std::fmod(target - current + PI * 3.0f, TWO_PI) - PI;
        if (diff < -PI) diff += TWO_PI;
        return current + diff * k;
    }

} // namespace PhysicsBlock
