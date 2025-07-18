#include "PhysicsWorld.hpp"
#include "BaseCalculate.hpp"
#include "MapStatic.hpp"
#include <algorithm>
#include <map>
#include <iostream>

namespace PhysicsBlock
{

    inline void PhysicsWorld::Arbiter(PhysicsShape *S, PhysicsParticle *P)
    {
        BaseArbiter *ptr;
#if MemoryPoolBool
        ptr = PoolPhysicsArbiterSP.newElement(S, P);
        ptr->key.PoolID = 0;
#else
        ptr = new PhysicsArbiterSP(S, P);
#endif
        HandleCollideGroup(ptr);
    }
    inline void PhysicsWorld::Arbiter(PhysicsShape *S1, PhysicsShape *S2)
    {
        BaseArbiter *ptr;
#if MemoryPoolBool
        ptr = PoolPhysicsArbiterSS.newElement(S1, S2);
        ptr->key.PoolID = 1;
#else
        ptr = new PhysicsArbiterSS(S1, S2);
#endif
        HandleCollideGroup(ptr);
    }
    inline void PhysicsWorld::Arbiter(PhysicsShape *S, MapFormwork *M)
    {
        BaseArbiter *ptr;
#if MemoryPoolBool
        ptr = PoolPhysicsArbiterS.newElement(S, M);
        ptr->key.PoolID = 2;
#else
        ptr = new PhysicsArbiterS(S, M);
#endif
        HandleCollideGroup(ptr);
    }
    inline void PhysicsWorld::Arbiter(PhysicsParticle *P, MapFormwork *M)
    {
        BaseArbiter *ptr;
#if MemoryPoolBool
        ptr = PoolPhysicsArbiterP.newElement(P, M);
        ptr->key.PoolID = 3;
#else
        ptr = new PhysicsArbiterP(P, M);
#endif
        HandleCollideGroup(ptr);
    }

    inline void PhysicsWorld::DeleteArbiter(BaseArbiter *BA)
    {
#if MemoryPoolBool
        if (BA->key.PoolID < 2)
        {
            if (BA->key.PoolID == 0)
            {
                PoolPhysicsArbiterSP.deleteElement((PhysicsArbiterSP *)BA);
            }
            else
            {
                PoolPhysicsArbiterSS.deleteElement((PhysicsArbiterSS *)BA);
            }
        }
        else
        {
            if (BA->key.PoolID == 2)
            {
                PoolPhysicsArbiterS.deleteElement((PhysicsArbiterS *)BA);
            }
            else
            {
                PoolPhysicsArbiterP.deleteElement((PhysicsArbiterP *)BA);
            }
        }
#else
        delete BA;
#endif
    }

    PhysicsWorld::PhysicsWorld(glm::dvec2 gravityAcceleration, const bool wind) : GravityAcceleration(gravityAcceleration), WindBool(wind)
    {
    }

    PhysicsWorld::~PhysicsWorld()
    {
        if (GridWind != nullptr)
        {
            delete GridWind;
        }
        if (wMapFormwork != nullptr)
        {
            delete (MapStatic *)wMapFormwork; // 暂时
        }
        // 物理形状
        for (auto i : PhysicsShapeS)
        {
            delete i;
        }
        // 物理点
        for (auto i : PhysicsParticleS)
        {
            delete i;
        }
        // 物理关节
        for (auto i : PhysicsJointS)
        {
            delete i;
        }
        // 物理绳子
        for (auto i : BaseJunctionS)
        {
            delete i;
        }
        // 两个碰撞对象间的信息
        for (auto i : CollideGroupS)
        {
            DeleteArbiter(i.second);
        }
    }

    inline void PhysicsWorld::HandleCollideGroup(BaseArbiter *Ba)
    {
        if (Ba->numContacts > 0)
        {
            if (CollideGroupS.find(Ba->key) == CollideGroupS.end())
            {
                CollideGroupS.insert({Ba->key, Ba});
            }
            else
            {
                CollideGroupS[Ba->key]->Update(Ba->contacts, Ba->numContacts);
                DeleteArbiter(Ba);
            }
        }
        else
        {
            CollideGroupS.erase(Ba->key);
            DeleteArbiter(Ba);
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
        for (size_t i = 0; i < PhysicsShapeS.size(); ++i)
        {
            for (auto o : PhysicsParticleS)
            {
                // 静止了，跳过碰撞遍历
                if (o->StaticNum > 10)
                {
                    continue;
                }
                if (PhysicsShapeS[i]->DropCollision(o->pos).Collision)
                {
                    o->OldPosUpDataBool = false; // 关闭旧位置更新
#if ThreadPoolBool
                    const auto Fun = [](PhysicsWorld *W, PhysicsShape *S, PhysicsParticle *P)
                    { W->Arbiter(S, P); };
                    mThreadPool.enqueue(Fun, this, PhysicsShapeS[i], o);
#else
                    Arbiter(PhysicsShapeS[i], o);
#endif
                }
                else
                {
                    CollideGroupS.erase(ArbiterKey(PhysicsShapeS[i], o));
                }
            }

            // 静止了，跳过碰撞遍历
            if (PhysicsShapeS[i]->StaticNum > 10)
            {
                continue;
            }

            for (size_t j = 0; j < PhysicsShapeS.size(); ++j)
            {
                if (i == j)
                {
                    continue;
                }
                if ((PhysicsShapeS[i]->CollisionR + PhysicsShapeS[j]->CollisionR) < Modulus(PhysicsShapeS[i]->pos - PhysicsShapeS[j]->pos))
                {
                    CollideGroupS.erase(ArbiterKey(PhysicsShapeS[i], PhysicsShapeS[j]));
                    continue;
                }
#if ThreadPoolBool
                const auto Fun = [](PhysicsWorld *W, PhysicsShape *S1, PhysicsShape *S2)
                { W->Arbiter(S1, S2); };
                mThreadPool.enqueue(Fun, this, PhysicsShapeS[i], PhysicsShapeS[j]);
#else
                Arbiter(PhysicsShapeS[i], PhysicsShapeS[j]);
#endif
            }
        }

        // 处理 网格形状 和 地形的碰撞
        for (auto o : PhysicsShapeS)
        {
            // 静止了，跳过碰撞遍历
            if (o->StaticNum > 10)
            {
                continue;
            }
#if ThreadPoolBool
            const auto Fun = [](PhysicsWorld *W, PhysicsShape *S, MapFormwork *M)
            { W->Arbiter(S, M); };
            mThreadPool.enqueue(Fun, this, o, wMapFormwork);
#else
            Arbiter(o, wMapFormwork);
#endif
        }

        // 处理 点 和 地形的碰撞
        for (auto o : PhysicsParticleS)
        {
            // 静止了，跳过碰撞遍历
            if (o->StaticNum > 10)
            {
                continue;
            }
#if ThreadPoolBool
            const auto Fun = [](PhysicsWorld *W, PhysicsParticle *P, MapFormwork *M)
            { W->Arbiter(P, M); };
            mThreadPool.enqueue(Fun, this, o, wMapFormwork);
#else
            Arbiter(o, wMapFormwork);
#endif
        }

#if ThreadPoolBool
        while (mThreadPool.TasksEnd())
            ;
#endif

        // 预处理
        double inv_dt = 1.0 / time;
        for (auto kv : CollideGroupS)
        {
            kv.second->PreStep(inv_dt);
        }
        for (auto J : PhysicsJointS)
        {
            J->PreStep(inv_dt);
        }
        for (auto J : BaseJunctionS)
        {
            J->PreStep(inv_dt);
        }

        // 迭代结果
        for (size_t i = 0; i < 10; ++i)
        {
            for (auto kv : CollideGroupS)
            {
                kv.second->ApplyImpulse();
            }
            for (auto J : PhysicsJointS)
            {
                J->ApplyImpulse();
            }
            for (auto J : BaseJunctionS)
            {
                J->ApplyImpulse();
            }
        }

        // 移动
        for (auto i : PhysicsShapeS)
        {
            i->PhysicsPos(time, GravityAcceleration);
        }
        for (auto i : PhysicsParticleS)
        {
            if (wMapFormwork->FMGetCollide(i->pos))
            {
                i->OldPosUpDataBool = false; // 关闭旧位置更新
            }
            i->PhysicsPos(time, GravityAcceleration);
        }
    }

    void PhysicsWorld::PhysicsInformationUpdate()
    {
        // 碰撞检测
        for (size_t i = 0; i < PhysicsShapeS.size(); ++i)
        {
            for (auto o : PhysicsParticleS)
            {
                if (PhysicsShapeS[i]->DropCollision(o->pos).Collision)
                {
                    o->OldPosUpDataBool = false; // 关闭旧位置更新
                    Arbiter(PhysicsShapeS[i], o);
                }
                else
                {
                    CollideGroupS.erase(ArbiterKey(PhysicsShapeS[i], o));
                }
            }

            if (i == (PhysicsShapeS.size() - 1))
            {
                continue;
            }
            for (size_t j = i + 1; j < PhysicsShapeS.size(); ++j)
            {
                if ((PhysicsShapeS[i]->CollisionR + PhysicsShapeS[j]->CollisionR) < Modulus(PhysicsShapeS[i]->pos - PhysicsShapeS[j]->pos))
                {
                    CollideGroupS.erase(ArbiterKey(PhysicsShapeS[i], PhysicsShapeS[j]));
                    continue;
                }
                Arbiter(PhysicsShapeS[i], PhysicsShapeS[j]);
            }
        }

        // 处理 网格形状 和 地形的碰撞
        for (auto o : PhysicsShapeS)
        {
            o->OldPos = o->pos;
            Arbiter(o, wMapFormwork);
        }

        // 处理 点 和 地形的碰撞
        for (auto o : PhysicsParticleS)
        {
            o->OldPos = o->pos;
            Arbiter(o, wMapFormwork);
        }

        // 预处理
        for (auto kv : CollideGroupS)
        {
            kv.second->PreStep();
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
