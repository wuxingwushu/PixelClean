#include "PhysicsJunction.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    PhysicsJunction::PhysicsJunction(CordKnot object1, CordKnot object2, const CordType Type)
        : objectA(object1), objectB(object2), type(Type)
    {
        glm::dvec2 distance = objectB.ptr->PFGetPos() - objectA.ptr->PFGetPos();
        Length = Modulus(distance);
        KnotSize = ToInt(Length) * 2 + 1;
        distance /= (KnotSize + 1);
        Length /= (KnotSize + 1);
        PhysicsParticleS = (PhysicsParticle *)new char[sizeof(PhysicsParticle) * KnotSize];
        for (size_t i = 0; i < KnotSize; ++i)
        {
            new (&PhysicsParticleS[i]) PhysicsParticle(glm::dvec2(objectA.ptr->PFGetPos() + distance + (distance * (double)i)), 0.1);
        }
    }

    PhysicsJunction::~PhysicsJunction()
    {
        for (size_t i = 0; i < KnotSize; ++i)
        {
            PhysicsParticleS[i].~PhysicsParticle();
        }
        delete PhysicsParticleS;
    }

    void PhysicsJunction::PhysicsAnalytic(glm::dvec2 pos, PhysicsParticle *B)
    {
        glm::dvec2 distance = pos - B->pos;
        double distanceVal = Modulus(distance);
        distance /= distanceVal;
        distanceVal -= Length;
        B->AddForce(distance * coefficient * -distanceVal);
    }

    void PhysicsJunction::PhysicsAnalytic(PhysicsParticle *A, PhysicsParticle *B)
    {
        glm::dvec2 distance = A->pos - B->pos;
        double distanceVal = Modulus(distance);
        distance /= distanceVal;
        distanceVal -= Length;
        A->AddForce(distance * coefficient * distanceVal);
        B->AddForce(distance * coefficient * -distanceVal);
    }

    void PhysicsJunction::PhysicsAnalytic(PhysicsShape *A, glm::dvec2 Arm, PhysicsParticle *B)
    {
        Arm = vec2angle(Arm, A->angle);
        Arm = A->pos + Arm;
        glm::dvec2 distance = A->pos + Arm - B->pos;
        double distanceVal = Modulus(distance);
        distance /= distanceVal;
        distanceVal -= Length;
        A->AddForce(Arm, distance * coefficient * distanceVal);
        B->AddForce(distance * coefficient * -distanceVal);
    }

    void PhysicsJunction::BearForceAnalytic()
    {
        if (objectA.ptr != nullptr)
        {
            if (objectA.ptr->PFGetType() == PhysicsObjectEnum::particle)
            {
                PhysicsAnalytic((PhysicsParticle *)objectA.ptr, &PhysicsParticleS[0]);
            }
            else
            {
                PhysicsAnalytic((PhysicsShape *)objectA.ptr, objectA.relativePos, &PhysicsParticleS[0]);
            }
        }
        else
        {
            PhysicsAnalytic(objectA.relativePos, &PhysicsParticleS[0]);
        }
        if (objectB.ptr != nullptr)
        {
            if (objectB.ptr->PFGetType() == PhysicsObjectEnum::particle)
            {
                PhysicsAnalytic((PhysicsParticle *)objectB.ptr, &PhysicsParticleS[KnotSize - 1]);
            }
            else
            {
                PhysicsAnalytic((PhysicsShape *)objectB.ptr, objectB.relativePos, &PhysicsParticleS[KnotSize - 1]);
            }
        }
        else
        {
            PhysicsAnalytic(objectA.relativePos, &PhysicsParticleS[KnotSize - 1]);
        }
        for (size_t i = 0; i < KnotSize - 1; ++i)
        {
            PhysicsAnalytic(&PhysicsParticleS[i], &PhysicsParticleS[i + 1]);
        }
    }

}
