#include "MovementComponent.h"

namespace GAME {

    MovementComponent::MovementComponent(PhysicsBlock::PhysicsFormwork* body)
        : mBody(body)
    {
    }

    // =========================================================================
    // 意图接口（每帧由上层调用）
    // =========================================================================

    void MovementComponent::SetMoveInput(const Vec2_& worldDir)
    {
        // 累加方向投票（适配"KeyDown 按住每帧触发"的输入模型）
        mMoveInput.x += worldDir.x;
        mMoveInput.y += worldDir.y;
    }

    void MovementComponent::SetLookAngle(float targetAngle)
    {
        mDesiredAngle = targetAngle;
        mHasLookTarget = true;
    }

    void MovementComponent::ClearIntent()
    {
        mMoveInput = {0, 0};
        mDesiredSpeed = {0, 0};
        mHasLookTarget = false;
    }

    // =========================================================================
    // 模式控制
    // =========================================================================

    void MovementComponent::SetMode(MovementMode mode)
    {
        if (mMode == mode) return;
        // 从 Ragdoll/Frozen 切到 Controlled 时，把当前速度方向作为初始目标，
        // 避免"急刹车"突兀（R4 风险对策）
        if (mode == MovementMode::Controlled &&
            (mMode == MovementMode::Ragdoll || mMode == MovementMode::Frozen))
        {
            Vec2_ v = mBody->PFSpeed();
            float len = std::sqrt(v.x * v.x + v.y * v.y);
            if (len > mConfig.MaxSpeed)
            {
                // 残留速度超过上限：保留方向但截断到 MaxSpeed，让 ComputeMoveForce 平滑降速
                mDesiredSpeed = { v.x / len * mConfig.MaxSpeed, v.y / len * mConfig.MaxSpeed };
            }
            else
            {
                // 残留速度在上限内：暂时保留，下一帧上层会重新 SetMoveInput 覆盖
                mDesiredSpeed = v;
            }
        }
        if (mode == MovementMode::Ragdoll)
        {
            mRagdollTimer = 0.0f; // 进入 Ragdoll 重新计时
        }
        mMode = mode;
    }

    // =========================================================================
    // 每帧更新（统一施力入口，在 PhysicsEmulator 之前调用）
    // =========================================================================

    void MovementComponent::Update(float dt)
    {
        switch (mMode)
        {
        case MovementMode::Controlled: UpdateControlled(dt); break;
        case MovementMode::Ragdoll:    UpdateRagdoll(dt);    break;
        case MovementMode::Frozen:     UpdateFrozen(dt);     break;
        }
        // 本帧意图消费完毕，清零，等待下一帧上层重新设置
        ClearIntent();
    }

    // ----- Controlled：走位态 -----
    void MovementComponent::UpdateControlled(float dt)
    {
        // 由方向投票算出目标速度（归一化 × MaxSpeed）
        float len = std::sqrt(mMoveInput.x * mMoveInput.x + mMoveInput.y * mMoveInput.y);
        if (len > 1e-4f)
        {
            mDesiredSpeed = { mMoveInput.x / len * mConfig.MaxSpeed,
                              mMoveInput.y / len * mConfig.MaxSpeed };
        }
        else
        {
            // 无输入 → 目标速度归零（松键即停，靠 ComputeMoveForce 反推的减速力实现）
            mDesiredSpeed = {0, 0};
        }

        // —— 平移：反推施力（自动尊重 mass/kinematic，且已补偿阻尼）——
        FLOAT_ mass = mBody->PFGetMass();
        // mass 为 FLOAT_MAX 表示静态体，跳过施力
        if (mass > 0 && mass < FLOAT_MAX)
        {
            Vec2_ currentSpeed = mBody->PFSpeed();

            // ★ 用 Acceleration 限制本帧速度变化量，实现平滑加速/减速
            Vec2_ deltaV = { mDesiredSpeed.x - currentSpeed.x,
                             mDesiredSpeed.y - currentSpeed.y };
            float deltaLen = std::sqrt(deltaV.x * deltaV.x + deltaV.y * deltaV.y);
            float maxDelta = mConfig.Acceleration * dt;  // 本帧允许的最大速度变化（像素/秒）
            if (deltaLen > maxDelta)
            {
                // 截断到允许的变化量，保持方向不变
                deltaV = { deltaV.x / deltaLen * maxDelta,
                           deltaV.y / deltaLen * maxDelta };
            }

            // 本帧实际要达到的中间速度 = 当前速度 + 受限的速度变化
            Vec2_ targetForThisFrame = { currentSpeed.x + deltaV.x,
                                         currentSpeed.y + deltaV.y };

            Vec2_ f = PhysicsBlock::ComputeMoveForce(
                currentSpeed, targetForThisFrame, mass, dt);
            mBody->AddForce(f);
        }

        // —— 朝向：指数平滑（消除瞬切，走最短弧）——
        if (mHasLookTarget)
        {
            float k = PhysicsBlock::SmoothK(mConfig.TurnRate, dt);
            float currentAngle = mBody->PFGetAngle();
            mBody->PFSetAngle(PhysicsBlock::LerpAngle(currentAngle, mDesiredAngle, k));
        }
    }

    // ----- Ragdoll：被击飞态 -----
    void MovementComponent::UpdateRagdoll(float dt)
    {
        // 不施加任何力，让物理自然演化（被击飞、被重力影响、被碰撞弹开）
        mRagdollTimer += dt;
        if (mRagdollTimer < mConfig.RagdollMinTime) return;
        // 速度衰减到阈值以下 → 切回 Controlled
        Vec2_ v = mBody->PFSpeed();
        float speedLen = std::sqrt(v.x * v.x + v.y * v.y);
        if (speedLen < mConfig.RagdollRecoverSpeed)
        {
            SetMode(MovementMode::Controlled);
        }
    }

    // ----- Frozen：硬直态 -----
    void MovementComponent::UpdateFrozen(float dt)
    {
        // 硬直：目标速度归零（让摩擦/阻尼把它停下来），不主动施力
        mDesiredSpeed = {0, 0};
        (void)dt;
    }

} // namespace GAME
