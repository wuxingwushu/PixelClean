#include "PhysicsJunction.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    /** @brief 构造两个角度物体间的连接约束
     *  @details 记录两端物体的指针和局部连接臂，并根据两端连接点的世界坐标距离
     *           计算初始静止长度 Length
     *  @param Particle1 形状物体1
     *  @param arm1 物体1上的连接臂（局部坐标）
     *  @param Particle2 形状物体2
     *  @param arm2 物体2上的连接臂（局部坐标）
     *  @param type 连接类型（绳子/弹簧/杠杆/橡皮筋） */
    PhysicsJunctionSS::PhysicsJunctionSS(PhysicsAngle *Particle1, Vec2_ arm1, PhysicsAngle *Particle2, Vec2_ arm2, CordType type)
        : mParticle1(Particle1), mArm1(arm1), mParticle2(Particle2), mArm2(arm2)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    /** @brief 析构函数，清理资源 */
    PhysicsJunctionSS::~PhysicsJunctionSS()
    {
    }

    /** @brief 预处理阶段，计算约束的法线方向、位置偏差和世界坐标下的连接臂向量
     *  @details 计算步骤：
     *           1. 计算 A→B 方向并归一化得到 Normal
     *           2. 计算当前距离与静止长度的差值 L - Length
     *           3. 使用 Baumgarte 稳定化计算 bias
     *           4. 根据绳子类型修正 bias
     *           5. 更新世界坐标系下的连接臂 mR1、mR2
     *           6. 计算冲量因子 k（有效质量倒数）
     *  @param inv_dt 时间步长的倒数 */
    void PhysicsJunctionSS::PreStep(FLOAT_ inv_dt)
    {
        Vec2_ dp = GetA() - GetB();
        FLOAT_ L = Modulus(dp);
        Normal = dp / L;
        L = L - Length;
        bias = -0.2 * inv_dt * L;
        biasType();

        mR1 = vec2angle(mArm1, mParticle1->angle);
        mR2 = vec2angle(mArm2, mParticle2->angle);
        
        // 计算冲量因子（考虑质量和转动惯量）
        k = mParticle1->invMass + mParticle2->invMass;
        FLOAT_ r1CrossN = Cross(mR1, Normal);
        FLOAT_ r2CrossN = Cross(mR2, Normal);
        k += mParticle1->invMomentInertia * r1CrossN * r1CrossN;
        k += mParticle2->invMomentInertia * r2CrossN * r2CrossN;
        
        /*Vec2_ impulse = Normal * bias * JISHU;
        mParticle1->speed += mParticle1->invMass * impulse;
        mParticle1->angleSpeed += mParticle1->invMomentInertia * Cross(arm1, impulse);
        mParticle2->speed -= mParticle2->invMass * impulse;
        mParticle2->angleSpeed -= mParticle2->invMomentInertia * Cross(arm2, impulse);
        */
    }

    /** @brief 迭代应用冲量阶段，根据两物体的速度差计算并施加约束冲量
     *  @details 计算步骤：
     *           1. 计算两物体在连接点处的相对速度 dv
     *           2. 若 k==0（无穷大质量）则跳过
     *           3. 计算冲量大小：impulse = (dot(-dv*elasticity, Normal) + bias) / k
     *           4. 对物体1施加正冲量，对物体2施加反冲量
     *           5. 同时修正两物体的角速度 */
    void PhysicsJunctionSS::ApplyImpulse()
    {
        Vec2_ dv = mParticle1->speed + Cross(mParticle1->angleSpeed, mR1) - mParticle2->speed - Cross(mParticle2->angleSpeed, mR2);
        
        if (k == 0) return;
        
        FLOAT_ impulseMagnitude = (Dot(-dv * ElasticityType(), Normal) + bias) / k;
        Vec2_ impulse = Normal * impulseMagnitude;

        mParticle1->speed += mParticle1->invMass * impulse;
        mParticle1->angleSpeed += mParticle1->invMomentInertia * Cross(mR1, impulse);

        mParticle2->speed -= mParticle2->invMass * impulse;
        mParticle2->angleSpeed -= mParticle2->invMomentInertia * Cross(mR2, impulse);
    }

    /** @brief 构造角度物体到固定点的连接约束
     *  @details 记录物体指针和局部连接臂，并根据物体连接点与固定点之间的距离
     *           计算初始静止长度 Length
     *  @param Particle 形状物体
     *  @param arm 连接臂（局部坐标）
     *  @param RegularDrop 固定点位置
     *  @param type 连接类型 */
    PhysicsJunctionS::PhysicsJunctionS(PhysicsAngle *Particle, Vec2_ arm, Vec2_ RegularDrop, CordType type)
        : mParticle(Particle), Arm(arm), mRegularDrop(RegularDrop)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    /** @brief 析构函数，清理资源 */
    PhysicsJunctionS::~PhysicsJunctionS()
    {
    }

    /** @brief 预处理阶段，计算约束的法线方向、位置偏差和世界坐标下的连接臂向量
     *  @details 计算步骤：
     *           1. 计算 A→B 方向并归一化得到 Normal
     *           2. 计算当前距离与静止长度的差值 L - Length
     *           3. 使用 Baumgarte 稳定化计算 bias
     *           4. 根据绳子类型修正 bias
     *           5. 更新世界坐标系下的连接臂 R
     *           6. 计算冲量因子 k（考虑物体质量和转动惯量）
     *  @param inv_dt 时间步长的倒数 */
    void PhysicsJunctionS::PreStep(FLOAT_ inv_dt)
    {
        Vec2_ dp = GetA() - GetB();
        FLOAT_ L = Modulus(dp);
        Normal = dp / L;
        L = L - Length;
        bias = -0.2 * inv_dt * L;
        biasType();

        R = vec2angle(Arm, mParticle->angle);
        
        // 计算冲量因子（考虑质量和转动惯量）
        k = mParticle->invMass;
        FLOAT_ rCrossN = Cross(R, Normal);
        k += mParticle->invMomentInertia * rCrossN * rCrossN;
        
        // Vec2_ impulse = Normal * bias * JISHU;
        // mParticle->speed += mParticle->invMass * impulse;
        // mParticle->angleSpeed += mParticle->invMomentInertia * Cross(arm, impulse);
    }

    /** @brief 迭代应用冲量阶段，根据物体的速度计算并施加约束冲量
     *  @details 计算步骤：
     *           1. 计算物体在连接点处的线速度（含角速度分量）
     *           2. 若 k==0（无穷大质量）则跳过
     *           3. 计算冲量大小并施加到物体上
     *           4. 同时修正物体的角速度 */
    void PhysicsJunctionS::ApplyImpulse()
    {
        Vec2_ dv = mParticle->speed + Cross(mParticle->angleSpeed, R);
        
        if (k == 0) return;
        
        FLOAT_ impulseMagnitude = (Dot(-dv * ElasticityType(), Normal) + bias) / k;
        Vec2_ impulse = Normal * impulseMagnitude;

        mParticle->speed += mParticle->invMass * impulse;
        mParticle->angleSpeed += mParticle->invMomentInertia * Cross(R, impulse);
    }

    /** @brief 构造粒子到固定点的连接约束
     *  @details 记录粒子指针，并根据粒子位置与固定点之间的距离
     *           计算初始静止长度 Length
     *  @param Particle 粒子对象
     *  @param RegularDrop 固定点位置
     *  @param type 连接类型 */
    PhysicsJunctionP::PhysicsJunctionP(PhysicsParticle *Particle, Vec2_ RegularDrop, CordType type)
        : mParticle(Particle), mRegularDrop(RegularDrop)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    /** @brief 析构函数，清理资源 */
    PhysicsJunctionP::~PhysicsJunctionP()
    {
    }

    /** @brief 预处理阶段，计算约束的法线方向和位置偏差
     *  @details 计算步骤：
     *           1. 计算粒子到固定点的方向并归一化得到 Normal
     *           2. 计算当前距离与静止长度的差值 L - Length
     *           3. 使用 Baumgarte 稳定化计算 bias
     *           4. 根据绳子类型修正 bias
     *           5. 计算冲量因子 k（即粒子质量的倒数）
     *  @param inv_dt 时间步长的倒数 */
    void PhysicsJunctionP::PreStep(FLOAT_ inv_dt)
    {
        Vec2_ dp = GetA() - GetB();
        FLOAT_ L = Modulus(dp);
        Normal = dp / L;
        L = L - Length;
        bias = -0.2 * inv_dt * L;
        biasType();
        
        // 计算冲量因子（考虑质量）
        k = mParticle->invMass;

        // Vec2_ impulse = (Normal * bias * JISHU) /* + (Normal * mParticle->speed)*/;
        // mParticle->speed += mParticle->invMass * impulse;
    }

    /** @brief 迭代应用冲量阶段，根据粒子的速度计算并施加约束冲量
     *  @details 计算步骤：
     *           1. 若 k==0（无穷大质量）则跳过
     *           2. 计算冲量大小：impulse = (dot(-speed*elasticity, Normal) + bias) / k
     *           3. 将冲量施加到粒子上 */
    void PhysicsJunctionP::ApplyImpulse()
    {
        if (k == 0) return;
        
        FLOAT_ impulseMagnitude = (Dot((-mParticle->speed) * ElasticityType(), Normal) + bias) / k;
        Vec2_ impulse = Normal * impulseMagnitude;

        mParticle->speed += mParticle->invMass * impulse;
    }

    /** @brief 构造两个粒子间的连接约束
     *  @details 记录两粒子指针，并根据两粒子之间的距离
     *           计算初始静止长度 Length
     *  @param Particle1 粒子1
     *  @param Particle2 粒子2
     *  @param type 连接类型 */
    PhysicsJunctionPP::PhysicsJunctionPP(PhysicsParticle *Particle1, PhysicsParticle *Particle2, CordType type)
        : mParticle1(Particle1), mParticle2(Particle2)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    /** @brief 析构函数，清理资源 */
    PhysicsJunctionPP::~PhysicsJunctionPP()
    {
    }

    /** @brief 预处理阶段，计算约束的法线方向和位置偏差
     *  @details 计算步骤：
     *           1. 计算两粒子之间的方向并归一化得到 Normal
     *           2. 计算当前距离与静止长度的差值 L - Length
     *           3. 使用 Baumgarte 稳定化计算 bias
     *           4. 根据绳子类型修正 bias
     *           5. 计算冲量因子 k（两粒子质量倒数之和）
     *  @param inv_dt 时间步长的倒数 */
    void PhysicsJunctionPP::PreStep(FLOAT_ inv_dt)
    {
        Vec2_ dp = GetA() - GetB();
        FLOAT_ L = Modulus(dp);
        Normal = dp / L;
        L = L - Length;
        bias = -0.2 * inv_dt * L;
        biasType();
        
        // 计算冲量因子（考虑质量）
        k = mParticle1->invMass + mParticle2->invMass;

        // Vec2_ impulse = Normal * bias * JISHU;
        // mParticle1->speed += mParticle1->invMass * impulse;
        // mParticle2->speed -= mParticle2->invMass * impulse;
    }

    /** @brief 迭代应用冲量阶段，根据两粒子的速度差计算并施加约束冲量
     *  @details 计算步骤：
     *           1. 若 k==0 则跳过
     *           2. 计算冲量大小：impulse = (dot((v2-v1)*elasticity, Normal) + bias) / k
     *           3. 对粒子1施加正冲量，对粒子2施加反冲量 */
    void PhysicsJunctionPP::ApplyImpulse()
    {
        if (k == 0) return;
        
        FLOAT_ impulseMagnitude = (Dot((mParticle2->speed - mParticle1->speed) * ElasticityType(), Normal) + bias) / k;
        Vec2_ impulse = Normal * impulseMagnitude;

        mParticle1->speed += mParticle1->invMass * impulse;
        mParticle2->speed -= mParticle2->invMass * impulse;
    }

}