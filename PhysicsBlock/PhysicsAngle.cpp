#include "PhysicsAngle.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 构造函数
     * @param Pos 位置
     * @param Mass 质量
     * @param Friction 摩擦因数
     * @param Radius 碰撞半径
     * @details 初始化物理角度对象，设置位置、质量、摩擦因数和碰撞半径
     */
    PhysicsAngle::PhysicsAngle(Vec2_ Pos, FLOAT_ Mass, FLOAT_ Friction, FLOAT_ Radius) : PhysicsParticle(Pos, Mass, Friction), radius(Radius) {}
    
    /**
     * @brief 构造函数（无半径参数）
     * @param Pos 位置
     * @param Mass 质量
     * @param Friction 摩擦因数，默认为0.2
     * @details 初始化物理角度对象，设置位置、质量和摩擦因数
     */
    PhysicsAngle::PhysicsAngle(Vec2_ Pos, FLOAT_ Mass, FLOAT_ Friction) : PhysicsParticle(Pos, Mass, Friction) {}
    
    /**
     * @brief 构造函数（仅位置参数）
     * @param Pos 位置
     * @details 初始化物理角度对象，仅设置位置
     */
    PhysicsAngle::PhysicsAngle(Vec2_ Pos) : PhysicsParticle(Pos) {}
    
    /**
     * @brief 析构函数
     */
    PhysicsAngle::~PhysicsAngle() {}

    /**
     * @brief 受力（移动，旋转）
     * @param Pos 受力位置（世界位置）
     * @param Force 力向量
     * @details 1. 计算力臂（从质心到受力点的向量）
     * 2. 计算力臂的角度
     * 3. 计算力与力臂之间的夹角
     * 4. 计算力的大小
     * 5. 计算垂直于力臂的力分量（用于移动）
     * 6. 调用 PhysicsParticle::AddForce 添加移动力
     * 7. 计算扭矩并调用 AddTorque 添加旋转力
     */
    void PhysicsAngle::AddForce(Vec2_ Pos, Vec2_ Force)
    {
        if (invMass == 0)
        {
            return;
        }
        Vec2_ Arm = Pos - pos;
        PhysicsParticle::AddForce(Force);
        torque += Arm.x * Force.y - Arm.y * Force.x;
    }

    /**
     * @brief 受力（移动）
     * @param Force 力向量
     * @details 调用 PhysicsParticle::AddForce 添加移动力
     */
    void PhysicsAngle::AddForce(Vec2_ Force) { PhysicsParticle::AddForce(Force); }

    /**
     * @brief 受力（旋转）
     * @param ArmForce 力臂
     * @param Force 力大小
     * @details 计算扭矩并累加到当前扭矩
     */
    void PhysicsAngle::AddTorque(FLOAT_ ArmForce, FLOAT_ Force)
    {
        torque += ArmForce * Force; // 累加扭矩
    }

    /**
     * @brief 施加质心冲量
     * @param impulse 冲量向量
     * @details 瞬时改变线速度：speed += invMass * impulse；唤醒静止计数。
     * 与 PhysicsParticle::ApplyImpulse 一致，显式覆盖以便通过基类指针多态调用。 */
    void PhysicsAngle::ApplyImpulse(const Vec2_& impulse)
    {
        if (invMass == 0)
        {
            return;
        }
        speed += invMass * impulse;
        StaticNum = 0; // 唤醒
    }

    /**
     * @brief 施加带受力点的冲量
     * @param impulse 冲量向量
     * @param worldPoint 受力点（世界坐标）
     * @details 平移：speed += invMass * impulse（Δv = J/m）
     *          旋转：angleSpeed += invMomentInertia * Cross(r, impulse)（Δω = invI * (r × J)）
     *          其中 r = worldPoint - pos。复用碰撞求解器（PhysicsBaseArbiter）的现成公式。 */
    void PhysicsAngle::ApplyImpulse(const Vec2_& impulse, const Vec2_& worldPoint)
    {
        if (invMass == 0)
        {
            return;
        }
        speed += invMass * impulse;
        Vec2_ r = worldPoint - pos;
        angleSpeed += invMomentInertia * (r.x * impulse.y - r.y * impulse.x); // Cross(r, impulse)
        StaticNum = 0; // 唤醒
    }

    /**
     * @brief 施加纯角冲量
     * @param torqueImpulse 角冲量
     * @details 瞬时改变角速度：angleSpeed += invMomentInertia * torqueImpulse */
    void PhysicsAngle::ApplyTorqueImpulse(FLOAT_ torqueImpulse)
    {
        if (invMomentInertia == 0)
        {
            return;
        }
        angleSpeed += invMomentInertia * torqueImpulse;
        StaticNum = 0; // 唤醒
    }

    /**
     * @brief 根据外力和时间更新速度和角速度
     * @param time 时间差（秒）
     * @param Ga 重力加速度向量
     * @details 1. 调用 PhysicsParticle::PhysicsSpeed 更新线速度
     * 2. 应用最小损耗系数到角速度
     * 3. 根据扭矩更新角速度
     * 4. 重置扭矩为零
     */
    void PhysicsAngle::PhysicsSpeed(FLOAT_ time, Vec2_ Ga)
    {
        if (invMass == 0)
            return;

        PhysicsParticle::PhysicsSpeed(time, Ga);
#if Define_MinSpoilageBool
        angleSpeed *= Define_MinSpoilage;
#endif
        angleSpeed += time * invMomentInertia * torque;
        torque = 0;
    }

    /**
     * @brief 根据速度和时间更新位置和角度
     * @param time 时间差（秒）
     * @param Ga 重力加速度向量
     * @details 1. 调用 PhysicsParticle::PhysicsPos 更新位置
     * 2. 根据角速度更新角度
     */
    void PhysicsAngle::PhysicsPos(FLOAT_ time, Vec2_ Ga)
    {
        if (invMass == 0)
            return;

        PhysicsParticle::PhysicsPos(time, Ga);
        angle += time * angleSpeed;
    }

#if PhysicsBlock_Serialization
    /**
     * @brief 从 JSON 数据构造物理角度对象
     * @param data JSON 数据
     * @details 1. 调用 PhysicsParticle 的构造函数
     * 2. 从 JSON 数据中恢复转动惯量、角度、角速度、扭矩和碰撞半径
     * 3. 计算转动惯量倒数
     */
    PhysicsAngle::PhysicsAngle(const nlohmann::json_abi_v3_12_0::basic_json<> &data) : PhysicsParticle(data)
    {
        MomentInertia = data["MomentInertia"];
        if (MomentInertia == FLOAT_MAX) {
            invMomentInertia = 0;
        } else {
            invMomentInertia = FLOAT_(1) / MomentInertia;
        }
        angle = data["angle"];
        angleSpeed = data["angleSpeed"];
        torque = data["torque"];
        radius = data["radius"];
    }

    /**
     * @brief 将物理角度对象序列化为 JSON
     * @param data 输出的 JSON 数据
     * @details 1. 调用 PhysicsParticle::JsonSerialization
     * 2. 将转动惯量、角度、角速度、扭矩和碰撞半径序列化到 JSON 中
     */
    void PhysicsAngle::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        PhysicsParticle::JsonSerialization(data);
        data["MomentInertia"] = MomentInertia;
        data["angle"] = angle;
        data["angleSpeed"] = angleSpeed;
        data["torque"] = torque;
        data["radius"] = radius;
    }

    /**
     * @brief 从 JSON 数据反序列化物理角度对象
     * @param data JSON 数据
     * @details 1. 调用 PhysicsParticle::JsonContrarySerialization
     * 2. 从 JSON 数据中恢复转动惯量、角度、角速度、扭矩和碰撞半径
     * 3. 计算转动惯量倒数
     */
    void PhysicsAngle::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        PhysicsParticle::JsonContrarySerialization(data);
        MomentInertia = data["MomentInertia"];
        if (MomentInertia == FLOAT_MAX) {
            invMomentInertia = 0;
        }else{
            invMomentInertia = FLOAT_(1) / MomentInertia;
        }
        angle = data["angle"];
        angleSpeed = data["angleSpeed"];
        torque = data["torque"];
        radius = data["radius"];
    }
#endif

}