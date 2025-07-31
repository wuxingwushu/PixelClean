#include "PhysicsParticle.hpp"

namespace PhysicsBlock
{

    PhysicsParticle::PhysicsParticle(Vec2_ Pos, FLOAT_ Mass, FLOAT_ Friction) : OldPos(Pos), pos(Pos), mass(Mass), invMass(1.0 / Mass), friction(Friction)
    {
        if (mass == FLOAT_MAX) {
            invMass = 0;
        }
    }

    PhysicsParticle::PhysicsParticle(Vec2_ Pos) : OldPos(Pos), pos(Pos)
    {
    }

    PhysicsParticle::~PhysicsParticle()
    {
    }

    void PhysicsParticle::AddForce(Vec2_ Force)
    {
        if (invMass == 0) {
            return;
        }
        force += Force; // 累计力矩
    }

    void inline PhysicsParticle::PhysicsSpeed(FLOAT_ time, Vec2_ Ga)
    {
        if (invMass == 0)
		    return;

        speed += time * (Ga + invMass * force);
        force = { 0, 0 };
    }

    void inline PhysicsParticle::PhysicsPos(FLOAT_ time, Vec2_ Ga)
    {
        if (invMass == 0)
		    return;

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