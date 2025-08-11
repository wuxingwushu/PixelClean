#include "PhysicsCircle.hpp"

namespace PhysicsBlock
{

    PhysicsCircle::PhysicsCircle(Vec2_ Pos, FLOAT_ Radius, FLOAT_ Mass, FLOAT_ Friction)
    : PhysicsAngle(Pos, Mass, Friction), radius(Radius)
    {
        if (mass == FLOAT_MAX) {
            MomentInertia = FLOAT_MAX;
            invMomentInertia = 0;
        }else{
            MomentInertia = FLOAT_(0.5) * mass * radius * radius;
            invMomentInertia = FLOAT_(1.0) / MomentInertia;
        }
    }
    
    PhysicsCircle::~PhysicsCircle()
    {
    }

}
