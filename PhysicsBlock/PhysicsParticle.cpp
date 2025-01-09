#include "PhysicsParticle.hpp"

namespace PhysicsBlock
{

    PhysicsParticle::PhysicsParticle(glm::dvec2 Pos, double Mass) : OldPos(Pos), pos(Pos), mass(Mass), invMass(1.0 / Mass)
    {
    }

    PhysicsParticle::PhysicsParticle(glm::dvec2 Pos) : OldPos(Pos), pos(Pos)
    {
    }

    PhysicsParticle::~PhysicsParticle()
    {
    }

    void PhysicsParticle::AddForce(glm::dvec2 Force)
    {
        force += Force; // 累计力矩
    }

    void PhysicsParticle::PhysicsSpeed(double time, glm::dvec2 Ga)
    {
        speed += time * (Ga + invMass * force);
    }

    void PhysicsParticle::PhysicsPos(double time, glm::dvec2 Ga)
    {
        OldPos = pos;
        pos += time * speed;
    }

}