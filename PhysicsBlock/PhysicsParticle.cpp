#include "PhysicsParticle.hpp"

namespace PhysicsBlock
{

    PhysicsParticle::PhysicsParticle(Vec2_ Pos, FLOAT_ Mass, FLOAT_ Friction) : OldPos(Pos), pos(Pos), mass(Mass), invMass(1.0 / Mass), friction(Friction)
    {
        if (mass == FLOAT_MAX)
        {
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
        if (invMass == 0)
        {
            return;
        }
        force += Force; // 累计力矩
    }

    void inline PhysicsParticle::PhysicsSpeed(FLOAT_ time, Vec2_ Ga)
    {
        if (invMass == 0)
            return;

        speed += time * (Ga + invMass * force);
        force = {0, 0};
    }

    void inline PhysicsParticle::PhysicsPos(FLOAT_ time, Vec2_ Ga)
    {
        if (invMass == 0)
            return;

        if (OldPos == pos)
        {
            if (StaticNum < 200)
                ++StaticNum;
        }
        else
        {
            StaticNum = 0;
        }
        if (OldPosUpDataBool)
        {
            OldPos = pos;
        }
        pos += time * speed;
        OldPosUpDataBool = true;
    }
#if PhysicsBlock_Serialization
    PhysicsParticle::PhysicsParticle(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        JsonContrarySerialization(data);
    }

    void PhysicsParticle::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        SerializationVec2(data, pos);
        SerializationVec2(data, speed);
        SerializationVec2(data, force);
        data["mass"] = mass;
        data["friction"] = friction;
    }

    void PhysicsParticle::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        ContrarySerializationVec2(data, pos);
        ContrarySerializationVec2(data, speed);
        ContrarySerializationVec2(data, force);
        mass = data["mass"];
        if (mass == FLOAT_MAX)
        {
            invMass = 0;
        }
        else
        {
            invMass = FLOAT_(1) / mass;
        }
        friction = data["friction"];
    }
#endif
}