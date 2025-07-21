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
        mLockSP.lock();
        ptr = PoolPhysicsArbiterSP.newElement(S, P);
        mLockSP.unlock();
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
        mLockSS.lock();
        ptr = PoolPhysicsArbiterSS.newElement(S1, S2);
        mLockSS.unlock();
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
        mLockS.lock();
        ptr = PoolPhysicsArbiterS.newElement(S, M);
        mLockS.unlock();
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
        mLockP.lock();
        ptr = PoolPhysicsArbiterP.newElement(P, M);
        mLockP.unlock();
        ptr->key.PoolID = 3;
#else
        ptr = new PhysicsArbiterP(P, M);
#endif
        HandleCollideGroup(ptr);
    }
    inline void PhysicsWorld::Arbiter(PhysicsCircle *C, MapFormwork *M)
    {
        BaseArbiter *ptr;
#if MemoryPoolBool
        mLockC.lock();
        ptr = PoolPhysicsArbiterC.newElement(C, M);
        mLockC.unlock();
        ptr->key.PoolID = 4;
#else
        ptr = new PhysicsArbiterC(C, M);
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
                std::unique_lock<std::mutex> lock(mLockSP);
                PoolPhysicsArbiterSP.deleteElement((PhysicsArbiterSP *)BA);
            }
            else
            {
                std::unique_lock<std::mutex> lock(mLockSS);
                PoolPhysicsArbiterSS.deleteElement((PhysicsArbiterSS *)BA);
            }
        }
        else
        {
            if (BA->key.PoolID == 2)
            {
                std::unique_lock<std::mutex> lock(mLockS);
                PoolPhysicsArbiterS.deleteElement((PhysicsArbiterS *)BA);
            }
            else
            {
                std::unique_lock<std::mutex> lock(mLockP);
                PoolPhysicsArbiterP.deleteElement((PhysicsArbiterP *)BA);
            }
        }
        if (BA->key.PoolID == 4) {
            std::unique_lock<std::mutex> lock(mLockC);
            PoolPhysicsArbiterC.deleteElement((PhysicsArbiterC *)BA);
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
        Ba->ComputeCollide();
        if (Ba->numContacts > 0)
        {
            if (CollideGroupS.find(Ba->key) == CollideGroupS.end())
            {
#if ThreadPoolBool
                std::unique_lock<std::mutex> lock(mLock);
#endif
                CollideGroupS.insert({Ba->key, Ba});
            }
            else
            {
                CollideGroupS[Ba->key]->Update(Ba->contacts, Ba->numContacts);
                DeleteArbiter(Ba);
            }
            return;
        }
#if ThreadPoolBool
        std::unique_lock<std::mutex> lock(mLock);
#endif
        CollideGroupS.erase(Ba->key);
        DeleteArbiter(Ba);
    }

    void PhysicsWorld::PhysicsEmulator(double time)
    {
        inv_dt = 1.0 / time;
        // 外力改变
        for (auto i : PhysicsShapeS)
        {
            i->PhysicsSpeed(time, GravityAcceleration);
        }
        for (auto i : PhysicsParticleS)
        {
            i->PhysicsSpeed(time, GravityAcceleration);
        }
        for (auto o : PhysicsCircleS)
        {
            o->PhysicsSpeed(time, GravityAcceleration);
        }

        auto T_Fun = [this](int Index, int IndexEnd)
        {
            for (; Index < IndexEnd; ++Index)
            {
                for (auto o : PhysicsParticleS)
                {
                    // 静止了，跳过碰撞遍历
                    if (o->StaticNum > 10)
                    {
                        continue;
                    }
                    if (PhysicsShapeS[Index]->DropCollision(o->pos).Collision)
                    {
                        o->OldPosUpDataBool = false; // 关闭旧位置更新
                        Arbiter(PhysicsShapeS[Index], o);
                    }
                    else
                    {
#if ThreadPoolBool
                        mLock.lock();
#endif
                        CollideGroupS.erase(ArbiterKey(PhysicsShapeS[Index], o));
#if ThreadPoolBool
                        mLock.unlock();
#endif
                    }
                }

                // 静止了，跳过碰撞遍历
                if (PhysicsShapeS[Index]->StaticNum > 10)
                {
                    continue;
                }

                // 和地形的碰撞
                Arbiter(PhysicsShapeS[Index], wMapFormwork);

                // 和其他多边形的碰撞
                for (size_t j = 0; j < PhysicsShapeS.size(); ++j)
                {
                    if (Index == j)
                    {
                        continue;
                    }
                    if ((PhysicsShapeS[Index]->CollisionR + PhysicsShapeS[j]->CollisionR) < Modulus(PhysicsShapeS[Index]->pos - PhysicsShapeS[j]->pos))
                    {
#if ThreadPoolBool
                        mLock.lock();
#endif
                        CollideGroupS.erase(ArbiterKey(PhysicsShapeS[Index], PhysicsShapeS[j]));
#if ThreadPoolBool
                        mLock.unlock();
#endif
                        continue;
                    }
                    Arbiter(PhysicsShapeS[Index], PhysicsShapeS[j]);
                }
            }
        };

#if ThreadPoolBool
        std::vector<std::future<void>> xTn;
        const int xThreadNum = 4;
        int SizeD = PhysicsShapeS.size() / xThreadNum;
        int SizeY = PhysicsShapeS.size() % xThreadNum;
        for (size_t i = 0; i < xThreadNum; ++i)
        {
            if (i < SizeY) {
                int s = (SizeD * i + i);
                int e = (SizeD * i + i) + (SizeD + 1);
                xTn.push_back(mThreadPool.enqueue(T_Fun, (SizeD * i + i), (SizeD * i + i) + (SizeD + 1)));
            } else {
                int s = (SizeD * i + SizeY);
                int e = (SizeD * i + SizeY) + (SizeD);
                xTn.push_back(mThreadPool.enqueue(T_Fun, (SizeD * i + SizeY), (SizeD * i + SizeY) + SizeD));
            }
        }
#else
        // 碰撞检测
        T_Fun(0, PhysicsShapeS.size());
#endif

        
        // 处理 点 和 地形的碰撞
        for (auto o : PhysicsParticleS)
        {
            // 静止了，跳过碰撞遍历
            if (o->StaticNum > 10)
            {
                continue;
            }

            Arbiter(o, wMapFormwork);
        }

        // 处理 圆 和 地形的碰撞
        for (auto o : PhysicsCircleS)
        {
            // 静止了，跳过碰撞遍历
            if (o->StaticNum > 10)
            {
                continue;
            }

            Arbiter(o, wMapFormwork);
        }
        

#if ThreadPoolBool
        // 等待任务结束
        for (auto& tf : xTn)
        {
            tf.wait();
        }
#endif

        // 预处理
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
        for (auto o : PhysicsCircleS)
        {
            o->PhysicsPos(time, GravityAcceleration);
        }
    }

    void PhysicsWorld::PhysicsInformationUpdate()
    {
        // 碰撞检测
        auto T_Fun = [this](int Index, int IndexEnd)
            {
                for (; Index < IndexEnd; ++Index)
                {
                    for (auto o : PhysicsParticleS)
                    {
                        if (PhysicsShapeS[Index]->DropCollision(o->pos).Collision)
                        {
                            o->OldPosUpDataBool = false; // 关闭旧位置更新
                            Arbiter(PhysicsShapeS[Index], o);
                        }
                        else
                        {
#if ThreadPoolBool
                            mLock.lock();
#endif
                            CollideGroupS.erase(ArbiterKey(PhysicsShapeS[Index], o));
#if ThreadPoolBool
                            mLock.unlock();
#endif
                        }
                    }

                    // 和地形的碰撞
                    Arbiter(PhysicsShapeS[Index], wMapFormwork);

                    // 和其他多边形的碰撞
                    for (size_t j = 0; j < PhysicsShapeS.size(); ++j)
                    {
                        if (Index == j)
                        {
                            continue;
                        }
                        if ((PhysicsShapeS[Index]->CollisionR + PhysicsShapeS[j]->CollisionR) < Modulus(PhysicsShapeS[Index]->pos - PhysicsShapeS[j]->pos))
                        {
#if ThreadPoolBool
                            mLock.lock();
#endif
                            CollideGroupS.erase(ArbiterKey(PhysicsShapeS[Index], PhysicsShapeS[j]));
#if ThreadPoolBool
                            mLock.unlock();
#endif
                            continue;
                        }
                        Arbiter(PhysicsShapeS[Index], PhysicsShapeS[j]);
                    }
                }
            };

#if ThreadPoolBool
        std::vector<std::future<void>> xTn;
        const int xThreadNum = 4;
        int SizeD = PhysicsShapeS.size() / xThreadNum;
        int SizeY = PhysicsShapeS.size() % xThreadNum;
        for (size_t i = 0; i < xThreadNum; ++i)
        {
            if (i < SizeY) {
                int s = (SizeD * i + i);
                int e = (SizeD * i + i) + (SizeD + 1);
                xTn.push_back(mThreadPool.enqueue(T_Fun, (SizeD * i + i), (SizeD * i + i) + (SizeD + 1)));
            }
            else {
                int s = (SizeD * i + SizeY);
                int e = (SizeD * i + SizeY) + (SizeD);
                xTn.push_back(mThreadPool.enqueue(T_Fun, (SizeD * i + SizeY), (SizeD * i + SizeY) + SizeD));
            }
        }
#else
        // 碰撞检测
        T_Fun(0, PhysicsShapeS.size());
#endif

#if ThreadPoolBool
        // 等待任务结束
        for (auto& tf : xTn)
        {
            tf.wait();
        }
#endif

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
