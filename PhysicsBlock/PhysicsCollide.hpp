#pragma once
#include "PhysicsParticle.hpp"
#include "PhysicsShape.hpp"
#include "MapFormwork.hpp"
#include "BaseArbiter.hpp"

namespace PhysicsBlock
{
    int Collide(Contact* contacts, PhysicsShape* A, PhysicsShape* B);

    int Collide(Contact* contacts, PhysicsShape* A, PhysicsParticle* B);

    int Collide(Contact* contacts, PhysicsShape* A, MapFormwork* B);

    int Collide(Contact* contacts, PhysicsParticle* A, MapFormwork* B);
}
