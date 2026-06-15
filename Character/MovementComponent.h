#pragma once
#include "../PhysicsBlock/PhysicsFormwork.hpp"
#include "../PhysicsBlock/MovementUtil.hpp"
#include <cmath>

namespace GAME {

    // =========================================================================
    // MovementComponent —— 方案E 混合驱动移动组件
    //
    // 设计目标：把"玩家/NPC 移动"从原本的"PFSpeed() += 速度增量（伪力）"重构为
    // 统一的、可控的、物理自洽的三态驱动。玩家与 NPC 共用同一组件。
    //
    // 三个驱动模式（MovementMode）：
    //   - Controlled : 走位态。用 ComputeMoveForce 反推施力 → AddForce，追踪目标速度。
    //                  有最大速度上限、平滑加速、松键即停、朝向平滑跟随。
    //   - Ragdoll    : 被击飞态。完全不施力，交给物理引擎（重力/碰撞/阻尼）自然演化。
    //                  速度衰减到阈值以下后自动切回 Controlled（可配最短持续时间防抖）。
    //   - Frozen     : 硬直态。冻结移动（目标速度归零），常用于 NPC 受伤硬直。
    //                  可保留物理碰撞，但不主动施力。
    //
    // 上层只产生"意图"（SetMoveInput / SetLookAngle / SetMode），Update 集中施力。
    // =========================================================================

    // 驱动模式
    enum class MovementMode {
        Controlled,        // 走位：用 AddForce 追踪目标速度（手感精确）
        Ragdoll,           // 被击飞：完全交给物理（速度自然衰减后切回 Controlled）
        Frozen             // 硬直：冻结移动（清零目标速度，不主动施力）
    };

    // 配置参数（玩家/NPC 各持一份，可微调）
    struct MovementConfig {
        float MaxSpeed            = 120.0f;  // 目标速度上限（像素/秒）
        float TurnRate            = 12.0f;   // 朝向趋近率（消除 angle 瞬切；越大转向越快）
        float RagdollRecoverSpeed = 20.0f;   // Ragdoll 速度低于此值（像素/秒）切回 Controlled
        float RagdollMinTime      = 0.3f;    // Ragdoll 最短持续时间（秒，防抖）
    };

    class MovementComponent {
    public:
        // 构造：绑定一个物理体（不接管所有权，外部负责生命周期）
        explicit MovementComponent(PhysicsBlock::PhysicsFormwork* body);

        // ===== 上层"意图"接口（每帧调用，无副作用，仅记录意图）=====

        // 设置期望移动方向（世界坐标；零向量=停）。内部归一化后乘 MaxSpeed 得到目标速度。
        // 多次调用会累加方向投票（配合"KeyDown 每帧触发"的输入模型）。
        void SetMoveInput(const Vec2_& worldDir);

        // 设置期望朝向（弧度）。Update 时用 LerpAngle 平滑过渡，消除瞬切。
        void SetLookAngle(float targetAngle);

        // 清空本帧意图（在 Update 末尾由组件内部调用，外部一般不需要）
        void ClearIntent();

        // ===== 模式控制 =====
        void SetMode(MovementMode mode);
        MovementMode GetMode() const { return mMode; }

        // ===== 每帧更新（在 PhysicsEmulator 之前调用）=====
        // 把本帧意图翻译成物理操作（AddForce / 放手 / 冻结）。
        void Update(float dt);

        // ===== 配置 =====
        MovementConfig& Config() { return mConfig; }
        const MovementConfig& Config() const { return mConfig; }

    private:
        void UpdateControlled(float dt);
        void UpdateRagdoll(float dt);
        void UpdateFrozen(float dt);

        PhysicsBlock::PhysicsFormwork* mBody;
        MovementConfig  mConfig;
        MovementMode    mMode = MovementMode::Controlled;

        Vec2_           mMoveInput     = {0, 0};  // 本帧方向投票累加（来自 SetMoveInput）
        Vec2_           mDesiredSpeed  = {0, 0};  // 由 mMoveInput 归一化 × MaxSpeed 算出
        float           mDesiredAngle  = 0.0f;    // 本帧期望朝向
        bool            mHasLookTarget = false;   // 本帧是否设过朝向
        float           mRagdollTimer  = 0.0f;    // Ragdoll 持续计时
    };

} // namespace GAME
