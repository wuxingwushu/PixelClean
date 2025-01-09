#include "PhysicsWorld.hpp"
#include <algorithm>
#include "BaseCalculate.hpp"
#include <map>
#include <iostream>

namespace PhysicsBlock
{

    PhysicsWorld::PhysicsWorld(glm::dvec2 gravityAcceleration, const bool wind) : GravityAcceleration(gravityAcceleration), WindBool(wind)
    {
    }

    PhysicsWorld::~PhysicsWorld()
    {
        if (GridWind != nullptr)
        {
            delete GridWind;
        }
    }

    void PhysicsWorld::HandleCollideGroup(BaseArbiter *Ba)
    {
        if (Ba->numContacts > 0)
        {
            if (CollideGroupS.find(Ba->key) == CollideGroupS.end())
            {
                CollideGroupS.insert({ Ba->key, Ba });
            }
            else
            {
                CollideGroupS[Ba->key]->Update(Ba->contacts, Ba->numContacts);
            }
        }
        else
        {
            CollideGroupS.erase(Ba->key);
            delete Ba;
        }
    }

    void PhysicsWorld::PhysicsEmulator(double time)
    {
        // 外力改变
        for (auto i : PhysicsShapeS)
        {
            i->PhysicsSpeed(time, GravityAcceleration);
        }
        for (auto i : PhysicsParticleS)
        {
            i->PhysicsSpeed(time, GravityAcceleration);
        }

        // 碰撞检测
        for (size_t i = 0; i < PhysicsShapeS.size() - 1; ++i)
        {
            for (size_t j = i + 1; j < PhysicsShapeS.size(); ++j)
            {
                if ((PhysicsShapeS[i]->CollisionR + PhysicsShapeS[j]->CollisionR) < Modulus(PhysicsShapeS[i]->pos - PhysicsShapeS[j]->pos))
                {
                    continue;
                }
                BaseArbiter *ptr = new PhysicsArbiterSS(PhysicsShapeS[i], PhysicsShapeS[j]);
                HandleCollideGroup(ptr);
            }

            for (auto o : PhysicsParticleS)
            {
                if (PhysicsShapeS[i]->DropCollision(o->pos).Collision)
                {
                    BaseArbiter *ptr = new PhysicsArbiterSP(PhysicsShapeS[i], o);
                    HandleCollideGroup(ptr);
                }
            }
        }

        for (auto o : PhysicsShapeS)
        {
            BaseArbiter *ptr = new PhysicsArbiterS(o, wMapFormwork);
            HandleCollideGroup(ptr);
        }

        for (auto o : PhysicsParticleS)
        {
            BaseArbiter *ptr = new PhysicsArbiterP(o, wMapFormwork);
            HandleCollideGroup(ptr);
        }

        // 预处理
        for (auto kv : CollideGroupS) {
            kv.second->PreStep(time);
        }


        for (size_t i = 0; i < 100; i++)
        {
            for (auto kv : CollideGroupS) {
                kv.second->ApplyImpulse();
            }
        }

        // 移动
        for (auto i : PhysicsShapeS)
        {
            i->PhysicsPos(time, GravityAcceleration);
        }
        for (auto i : PhysicsParticleS)
        {
            i->PhysicsPos(time, GravityAcceleration);
        }
    }

    void PhysicsWorld::SetMapFormwork(MapFormwork *MapFormwork_)
    {
        wMapFormwork = MapFormwork_;
        if (!WindBool)
        {
            return;
        }
        GridWindSize = wMapFormwork->FMGetMapSize();
        if (GridWind != nullptr)
        {
            delete GridWind;
        }
        GridWind = new glm::dvec2[GridWindSize.x * GridWindSize.y];
        for (size_t i = 0; i < (GridWindSize.x * GridWindSize.y); i++)
        {
            GridWind[i] = glm::dvec2{0};
        }
    }

    PhysicsFormwork *PhysicsWorld::Get(glm::dvec2 pos)
    {
        for (auto i : PhysicsShapeS)
        {
            if ((Modulus(i->pos - pos) < i->PFGetCollisionR()) && (i->DropCollision(pos).Collision))
                return i;
        }
        for (auto i : PhysicsParticleS)
        {
            if (Modulus(i->pos - pos) < 0.25) // 点击位置距离点位置小于 0.25， 就判断选择 点
                return i;
        }
        return nullptr;
    }

    double PhysicsWorld::GetWorldEnergy()
    {
        double Energy = 0;
        for (auto i : PhysicsShapeS)
        {
            Energy += i->mass * ModulusLength(i->speed);
            Energy += i->MomentInertia * i->angleSpeed * i->angleSpeed;
        }
        for (auto i : PhysicsParticleS)
        {
            Energy += i->mass * ModulusLength(i->speed);
        }
        return Energy / 2;
    }
}
