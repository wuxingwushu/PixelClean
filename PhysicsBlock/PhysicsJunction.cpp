#include "PhysicsJunction.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    PhysicsJunctionSS::PhysicsJunctionSS(PhysicsShape *Particle1, Vec2_ arm1, PhysicsShape *Particle2, Vec2_ arm2, CordType type)
        : mParticle1(Particle1), mArm1(arm1), mParticle2(Particle2), mArm2(arm2)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    PhysicsJunctionSS::~PhysicsJunctionSS()
    {
    }

    // 预处理
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
        /*Vec2_ impulse = Normal * bias * JISHU;
        mParticle1->speed += mParticle1->invMass * impulse;
        mParticle1->angleSpeed += mParticle1->invMomentInertia * Cross(arm1, impulse);
        mParticle2->speed -= mParticle2->invMass * impulse;
        mParticle2->angleSpeed -= mParticle2->invMomentInertia * Cross(arm2, impulse);
        */
    }

    // 迭代出结果
    void PhysicsJunctionSS::ApplyImpulse()
    {
        Vec2_ dv = mParticle1->speed + Cross(mParticle1->angleSpeed, mR1) - mParticle2->speed - Cross(mParticle2->angleSpeed, mR2);

        Vec2_ impulse = Normal * (Dot(-dv * ElasticityType(), Normal) + bias);

        mParticle1->speed += mParticle1->invMass * impulse;
        mParticle1->angleSpeed += mParticle1->invMomentInertia * Cross(mR1, impulse);

        mParticle2->speed -= mParticle2->invMass * impulse;
        mParticle2->angleSpeed -= mParticle2->invMomentInertia * Cross(mR2, impulse);
    }

    PhysicsJunctionS::PhysicsJunctionS(PhysicsShape *Particle, Vec2_ arm, Vec2_ RegularDrop, CordType type)
        : mParticle(Particle), Arm(arm), mRegularDrop(RegularDrop)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    PhysicsJunctionS::~PhysicsJunctionS()
    {
    }

    // 预处理
    void PhysicsJunctionS::PreStep(FLOAT_ inv_dt)
    {
        Vec2_ dp = GetA() - GetB();
        FLOAT_ L = Modulus(dp);
        Normal = dp / L;
        L = L - Length;
        bias = -0.2 * inv_dt * L;
        biasType();

        R = vec2angle(Arm, mParticle->angle);
        // Vec2_ impulse = Normal * bias * JISHU;
        // mParticle->speed += mParticle->invMass * impulse;
        // mParticle->angleSpeed += mParticle->invMomentInertia * Cross(arm, impulse);
    }

    // 迭代出结果
    void PhysicsJunctionS::ApplyImpulse()
    {
        Vec2_ dv = mParticle->speed + Cross(mParticle->angleSpeed, R);

        Vec2_ impulse = Normal * (Dot(-dv * ElasticityType(), Normal) + bias);

        mParticle->speed += mParticle->invMass * impulse;
        mParticle->angleSpeed += mParticle->invMomentInertia * Cross(R, impulse);
    }

    PhysicsJunctionP::PhysicsJunctionP(PhysicsParticle *Particle, Vec2_ RegularDrop, CordType type)
        : mParticle(Particle), mRegularDrop(RegularDrop)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    PhysicsJunctionP::~PhysicsJunctionP()
    {
    }

    // 预处理
    void PhysicsJunctionP::PreStep(FLOAT_ inv_dt)
    {
        Vec2_ dp = GetA() - GetB();
        FLOAT_ L = Modulus(dp);
        Normal = dp / L;
        L = L - Length;
        bias = -0.2 * inv_dt * L;
        biasType();

        // Vec2_ impulse = (Normal * bias * JISHU) /* + (Normal * mParticle->speed)*/;
        // mParticle->speed += mParticle->invMass * impulse;
    }

    // 迭代出结果
    void PhysicsJunctionP::ApplyImpulse()
    {
        Vec2_ impulse = Normal * (Dot((-mParticle->speed) * ElasticityType(), Normal) + bias);

        mParticle->speed += mParticle->invMass * impulse;
    }

    PhysicsJunctionPP::PhysicsJunctionPP(PhysicsParticle *Particle1, PhysicsParticle *Particle2, CordType type)
        : mParticle1(Particle1), mParticle2(Particle2)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    PhysicsJunctionPP::~PhysicsJunctionPP()
    {
    }

    // 预处理
    void PhysicsJunctionPP::PreStep(FLOAT_ inv_dt)
    {
        Vec2_ dp = GetA() - GetB();
        FLOAT_ L = Modulus(dp);
        Normal = dp / L;
        L = L - Length;
        bias = -0.2 * inv_dt * L;
        biasType();

        // Vec2_ impulse = Normal * bias * JISHU;
        // mParticle1->speed += mParticle1->invMass * impulse;
        // mParticle2->speed -= mParticle2->invMass * impulse;
    }

    // 迭代出结果
    void PhysicsJunctionPP::ApplyImpulse()
    {
        Vec2_ impulse = Normal * (Dot((mParticle2->speed - mParticle1->speed) * ElasticityType(), Normal) + bias);

        mParticle1->speed += mParticle1->invMass * impulse;
        mParticle2->speed -= mParticle2->invMass * impulse;
    }

}
