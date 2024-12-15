#include "PhysicsParticle.hpp"

namespace PhysicsBlock
{

    PhysicsParticle::PhysicsParticle(glm::dvec2 Pos, double Mass) : pos(Pos), mass(Mass)
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

    void PhysicsParticle::PhysicsEmulator(double time, glm::dvec2 Ga)
    {
        glm::dvec2 AddSpeed = force / mass * time; // 增加的速度
        pos += (speed + (AddSpeed / 2.0)) * time;  // 移动后的位置
        speed += AddSpeed;                         // 将速度增加上去
        force = Ga * mass;                         // 清空受力 + 重力加速度
    }

    glm::dvec2 PhysicsParticle::PhysicsPlayact(double time, glm::dvec2 Ga)
    {
        glm::dvec2 AddSpeed = force / mass * time;      // 增加的速度
        return pos + (speed + (AddSpeed / 2.0)) * time; // 移动后的位置
    }

}