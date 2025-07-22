#include "PhysicsArbiter.hpp"
#include "PhysicsBaseCollide.hpp"

namespace PhysicsBlock
{

    void PhysicsArbiterSS::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsShape*)object1, (PhysicsShape*)object2);
    }

    void PhysicsArbiterSP::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsShape*)object1, (PhysicsParticle*)object2);
    }

    void PhysicsArbiterS::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsShape*)object1, (MapFormwork*)object2);
    }

    void PhysicsArbiterP::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsParticle*)object1, (MapFormwork*)object2);
    }

    void PhysicsArbiterC::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsCircle*)object1, (MapFormwork*)object2);
    }

    void PhysicsArbiterCS::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsCircle*)object1, (PhysicsShape*)object2);
    }

    void PhysicsArbiterCP::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsCircle*)object1, (PhysicsParticle*)object2);
    }

    void PhysicsArbiterCC::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsCircle*)object1, (PhysicsCircle*)object2);
    }

}