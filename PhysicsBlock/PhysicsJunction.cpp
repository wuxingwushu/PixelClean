#include "PhysicsJunction.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    /** @brief 构造两个角度物体间的连接约束
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

    /** @brief 析构函数 */
    PhysicsJunctionSS::~PhysicsJunctionSS()
    {
    }

    /** @brief 预处理阶段
     *  @details 计算约束的法线方向、位置偏差和世界坐标下的连接臂向量
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

    /** @brief 迭代应用冲量阶段
     *  @details 根据两物体的速度差计算并应用约束冲量，修正其运动状态 */
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

    /** @brief 析构函数 */
    PhysicsJunctionS::~PhysicsJunctionS()
    {
    }

    /** @brief 预处理阶段
     *  @details 计算约束的法线方向、位置偏差和世界坐标下的连接臂向量
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

    /** @brief 迭代应用冲量阶段
     *  @details 根据物体的速度计算并应用约束冲量，修正其运动状态 */
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
     *  @param Particle 粒子对象
     *  @param RegularDrop 固定点位置
     *  @param type 连接类型 */
    PhysicsJunctionP::PhysicsJunctionP(PhysicsParticle *Particle, Vec2_ RegularDrop, CordType type)
        : mParticle(Particle), mRegularDrop(RegularDrop)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    /** @brief 析构函数 */
    PhysicsJunctionP::~PhysicsJunctionP()
    {
    }

    /** @brief 预处理阶段
     *  @details 计算约束的法线方向和位置偏差
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

    /** @brief 迭代应用冲量阶段
     *  @details 根据粒子的速度计算并应用约束冲量 */
    void PhysicsJunctionP::ApplyImpulse()
    {
        if (k == 0) return;
        
        FLOAT_ impulseMagnitude = (Dot((-mParticle->speed) * ElasticityType(), Normal) + bias) / k;
        Vec2_ impulse = Normal * impulseMagnitude;

        mParticle->speed += mParticle->invMass * impulse;
    }

    /** @brief 构造两个粒子间的连接约束
     *  @param Particle1 粒子1
     *  @param Particle2 粒子2
     *  @param type 连接类型 */
    PhysicsJunctionPP::PhysicsJunctionPP(PhysicsParticle *Particle1, PhysicsParticle *Particle2, CordType type)
        : mParticle1(Particle1), mParticle2(Particle2)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    /** @brief 析构函数 */
    PhysicsJunctionPP::~PhysicsJunctionPP()
    {
    }

    /** @brief 预处理阶段
     *  @details 计算约束的法线方向和位置偏差
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

    /** @brief 迭代应用冲量阶段
     *  @details 根据两粒子的速度差计算并应用约束冲量 */
    void PhysicsJunctionPP::ApplyImpulse()
    {
        if (k == 0) return;
        
        FLOAT_ impulseMagnitude = (Dot((mParticle2->speed - mParticle1->speed) * ElasticityType(), Normal) + bias) / k;
        Vec2_ impulse = Normal * impulseMagnitude;

        mParticle1->speed += mParticle1->invMass * impulse;
        mParticle2->speed -= mParticle2->invMass * impulse;
    }

}