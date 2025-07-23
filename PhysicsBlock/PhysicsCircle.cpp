#include "PhysicsCircle.hpp"

namespace PhysicsBlock
{

    PhysicsCircle::PhysicsCircle(Vec2_ Pos, FLOAT_ Radius, FLOAT_ Mass)
    : PhysicsAngle(Pos, Mass), radius(Radius)
    {
    }
    
    PhysicsCircle::~PhysicsCircle()
    {
    }

    void inline PhysicsCircle::PhysicsSpeed(FLOAT_ time, Vec2_ Ga)
    {
        PhysicsParticle::PhysicsSpeed(time, Ga);
        angleSpeed += time * invMomentInertia * torque;
        torque = 0;
    }

    void inline PhysicsCircle::PhysicsPos(FLOAT_ time, Vec2_ Ga)
    {
        PhysicsParticle::PhysicsPos(time, Ga);
        angle += time * angleSpeed;
    }

}
