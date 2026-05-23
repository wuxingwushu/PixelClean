#include "PhysicsArbiter.hpp"
#include "PhysicsBaseCollide.hpp"

namespace PhysicsBlock
{

    void PhysicsArbiterSS::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsShape*)object1, (PhysicsShape*)object2);
        for (int i = 0; i < numContacts; ++i) {
            contacts[i].w_side = static_cast<unsigned char>(i);
        }
    }

    void PhysicsArbiterSP::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsShape*)object1, (PhysicsParticle*)object2);
        for (int i = 0; i < numContacts; ++i) {
            contacts[i].w_side = static_cast<unsigned char>(i);
        }
    }

    void PhysicsArbiterS::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsShape*)object1, (MapFormwork*)object2);
        for (int i = 0; i < numContacts; ++i) {
            contacts[i].w_side = static_cast<unsigned char>(i);
        }
    }

    void PhysicsArbiterP::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsParticle*)object1, (MapFormwork*)object2);
        for (int i = 0; i < numContacts; ++i) {
            contacts[i].w_side = static_cast<unsigned char>(i);
        }
    }

    void PhysicsArbiterC::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsCircle*)object1, (MapFormwork*)object2);
        for (int i = 0; i < numContacts; ++i) {
            contacts[i].w_side = static_cast<unsigned char>(i);
        }
    }

    void PhysicsArbiterCS::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsCircle*)object1, (PhysicsShape*)object2);
        for (int i = 0; i < numContacts; ++i) {
            contacts[i].w_side = static_cast<unsigned char>(i);
        }
    }

    void PhysicsArbiterCP::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsCircle*)object1, (PhysicsParticle*)object2);
        for (int i = 0; i < numContacts; ++i) {
            contacts[i].w_side = static_cast<unsigned char>(i);
        }
    }

    void PhysicsArbiterCC::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsCircle*)object1, (PhysicsCircle*)object2);
        for (int i = 0; i < numContacts; ++i) {
            contacts[i].w_side = static_cast<unsigned char>(i);
        }
    }

    void PhysicsArbiterLC::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsLine*)object1, (PhysicsCircle*)object2);
        for (int i = 0; i < numContacts; ++i) {
            contacts[i].w_side = static_cast<unsigned char>(i);
        }
    }

    void PhysicsArbiterLP::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsLine*)object1, (PhysicsParticle*)object2);
        for (int i = 0; i < numContacts; ++i) {
            contacts[i].w_side = static_cast<unsigned char>(i);
        }
    }

    void PhysicsArbiterLS::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsLine*)object1, (PhysicsShape*)object2);
        for (int i = 0; i < numContacts; ++i) {
            contacts[i].w_side = static_cast<unsigned char>(i);
        }
    }

    void PhysicsArbiterL::ComputeCollide()
    {
        numContacts = Collide(contacts, (PhysicsLine*)object1, (MapFormwork*)object2);
        for (int i = 0; i < numContacts; ++i) {
            contacts[i].w_side = static_cast<unsigned char>(i);
        }
    }

}