#include "PhysicsJunction.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    PhysicsJunctionSS::PhysicsJunctionSS(PhysicsShape *Particle1, glm::dvec2 arm1, PhysicsShape *Particle2, glm::dvec2 arm2, CordType type)
        : mParticle1(Particle1), mArm1(arm1), mParticle2(Particle2), mArm2(arm2)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    PhysicsJunctionSS::~PhysicsJunctionSS()
    {
    }

    // 预处理
    void PhysicsJunctionSS::PreStep(double inv_dt)
    {
        glm::dvec2 dp = GetA() - GetB();
        double L = Modulus(dp);
        Normal = dp / L;
        L = L - Length;
        bias = -0.2 * inv_dt * L;
        biasType();

        mR1 = vec2angle(mArm1, mParticle1->angle);
        mR2 = vec2angle(mArm2, mParticle2->angle);
        /*glm::dvec2 impulse = Normal * bias * JISHU;
        mParticle1->speed += mParticle1->invMass * impulse;
        mParticle1->angleSpeed += mParticle1->invMomentInertia * Cross(arm1, impulse);
        mParticle2->speed -= mParticle2->invMass * impulse;
        mParticle2->angleSpeed -= mParticle2->invMomentInertia * Cross(arm2, impulse);
        */
    }

    // 迭代出结果
    void PhysicsJunctionSS::ApplyImpulse()
    {
        glm::dvec2 dv = mParticle1->speed + Cross(mParticle1->angleSpeed, mR1) - mParticle2->speed - Cross(mParticle2->angleSpeed, mR2);

        glm::dvec2 impulse = Normal * (Dot(-dv * ElasticityType(), Normal) + bias);

        mParticle1->speed += mParticle1->invMass * impulse;
        mParticle1->angleSpeed += mParticle1->invMomentInertia * Cross(mR1, impulse);

        mParticle2->speed -= mParticle2->invMass * impulse;
        mParticle2->angleSpeed -= mParticle2->invMomentInertia * Cross(mR2, impulse);
    }

    PhysicsJunctionS::PhysicsJunctionS(PhysicsShape *Particle, glm::dvec2 arm, glm::dvec2 RegularDrop, CordType type)
        : mParticle(Particle), Arm(arm), mRegularDrop(RegularDrop)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    PhysicsJunctionS::~PhysicsJunctionS()
    {
    }

    // 预处理
    void PhysicsJunctionS::PreStep(double inv_dt)
    {
        glm::dvec2 dp = GetA() - GetB();
        double L = Modulus(dp);
        Normal = dp / L;
        L = L - Length;
        bias = -0.2 * inv_dt * L;
        biasType();

        R = vec2angle(Arm, mParticle->angle);
        // glm::dvec2 impulse = Normal * bias * JISHU;
        // mParticle->speed += mParticle->invMass * impulse;
        // mParticle->angleSpeed += mParticle->invMomentInertia * Cross(arm, impulse);
    }

    // 迭代出结果
    void PhysicsJunctionS::ApplyImpulse()
    {
        glm::dvec2 dv = mParticle->speed + Cross(mParticle->angleSpeed, R);

        glm::dvec2 impulse = Normal * (Dot(-dv * ElasticityType(), Normal) + bias);

        mParticle->speed += mParticle->invMass * impulse;
        mParticle->angleSpeed += mParticle->invMomentInertia * Cross(R, impulse);
    }

    PhysicsJunctionP::PhysicsJunctionP(PhysicsParticle *Particle, glm::dvec2 RegularDrop, CordType type)
        : mParticle(Particle), mRegularDrop(RegularDrop)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    PhysicsJunctionP::~PhysicsJunctionP()
    {
    }

    // 预处理
    void PhysicsJunctionP::PreStep(double inv_dt)
    {
        glm::dvec2 dp = GetA() - GetB();
        double L = Modulus(dp);
        Normal = dp / L;
        L = L - Length;
        bias = -0.2 * inv_dt * L;
        biasType();

        // glm::dvec2 impulse = (Normal * bias * JISHU) /* + (Normal * mParticle->speed)*/;
        // mParticle->speed += mParticle->invMass * impulse;
    }

    // 迭代出结果
    void PhysicsJunctionP::ApplyImpulse()
    {
        glm::dvec2 impulse = Normal * (Dot((-mParticle->speed) * ElasticityType(), Normal) + bias);

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
    void PhysicsJunctionPP::PreStep(double inv_dt)
    {
        glm::dvec2 dp = GetA() - GetB();
        double L = Modulus(dp);
        Normal = dp / L;
        L = L - Length;
        bias = -0.2 * inv_dt * L;
        biasType();

        // glm::dvec2 impulse = Normal * bias * JISHU;
        // mParticle1->speed += mParticle1->invMass * impulse;
        // mParticle2->speed -= mParticle2->invMass * impulse;
    }

    // 迭代出结果
    void PhysicsJunctionPP::ApplyImpulse()
    {
        glm::dvec2 impulse = Normal * (Dot((mParticle2->speed - mParticle1->speed) * ElasticityType(), Normal) + bias);

        mParticle1->speed += mParticle1->invMass * impulse;
        mParticle2->speed -= mParticle2->invMass * impulse;
    }

}
