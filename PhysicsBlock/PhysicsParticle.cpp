#include "PhysicsParticle.hpp"

namespace PhysicsBlock
{

    PhysicsParticle::PhysicsParticle(glm::dvec2 Pos, double Mass) : 
        pos(Pos), mass(Mass), invMass(1.0/Mass)
    {
    }

    PhysicsParticle::PhysicsParticle(glm::dvec2 Pos) : pos(Pos)
    {
    }

    PhysicsParticle::~PhysicsParticle()
    {
    }

    void PhysicsParticle::AddForce(glm::dvec2 Force)
    {
        force += Force; // 累计力矩
    }

    PhysicsState PhysicsParticle::PhysicsPlayact(double time, glm::dvec2 Ga)
    {
        glm::dvec2 AddSpeed = force / mass * time;      // 增加的速度
        return {pos + (speed + (AddSpeed / 2.0)) * time, speed + AddSpeed}; // 移动后的位置
    }

}