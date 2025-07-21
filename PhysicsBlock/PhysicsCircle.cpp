#include "PhysicsCircle.hpp"

namespace PhysicsBlock
{

    PhysicsCircle::PhysicsCircle(glm::dvec2 Pos, double Radius, double Mass)
    : PhysicsParticle(Pos, Mass), radius(Radius), angleSpeed(0)
    {
    }
    
    PhysicsCircle::~PhysicsCircle()
    {
    }

    void inline PhysicsCircle::PhysicsSpeed(double time, glm::dvec2 Ga)
    {
        PhysicsParticle::PhysicsSpeed(time, Ga);
        angleSpeed += time * invMomentInertia * torque;
        torque = 0;
    }

    void inline PhysicsCircle::PhysicsPos(double time, glm::dvec2 Ga)
    {
        PhysicsParticle::PhysicsPos(time, Ga);
        angle += time * angleSpeed;
    }

}
