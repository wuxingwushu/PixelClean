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

    void inline PhysicsParticle::PhysicsSpeed(double time, glm::dvec2 Ga)
    {
        if (invMass == 0)
		    return;

        speed += time * (Ga + invMass * force);
        force = { 0, 0 };
    }

    void inline PhysicsParticle::PhysicsPos(double time, glm::dvec2 Ga)
    {
        if (OldPos == pos) {
            if(StaticNum < 200)++StaticNum;
        }else{
            StaticNum = 0;
        }
        if (OldPosUpDataBool) { OldPos = pos; }
        pos += time * speed;
        OldPosUpDataBool = true;
    }

}