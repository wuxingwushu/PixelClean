#include "PhysicsWorld.hpp"
#include "BaseCalculate.hpp"
#include "MapStatic.hpp"
#include <algorithm>
#include <map>
#include <iostream>

namespace PhysicsBlock
{
    AuxiliaryArbiter(PhysicsArbiterCP, PhysicsCircle, C, PhysicsParticle, P, 0);
    AuxiliaryArbiter(PhysicsArbiterSP, PhysicsShape, S, PhysicsParticle, P, 1);
    AuxiliaryArbiter(PhysicsArbiterSS, PhysicsShape, S1, PhysicsShape, S2, 2);
    AuxiliaryArbiter(PhysicsArbiterS, PhysicsShape, S, MapFormwork, M, 3);
    AuxiliaryArbiter(PhysicsArbiterP, PhysicsParticle, P, MapFormwork, M, 4);
    AuxiliaryArbiter(PhysicsArbiterC, PhysicsCircle, C, MapFormwork, M, 5);
    AuxiliaryArbiter(PhysicsArbiterCS, PhysicsCircle, C, PhysicsShape, S, 6);
    AuxiliaryArbiter(PhysicsArbiterCC, PhysicsCircle, C1, PhysicsCircle, C2, 7);

    inline void PhysicsWorld::DeleteArbiter(BaseArbiter *BA)
    {
#if MemoryPoolBool
        switch (BA->key.PoolID)
        {
            AuxiliaryDelete(PhysicsArbiterCP, PhysicsCircle, C, PhysicsParticle, P, 0);
            AuxiliaryDelete(PhysicsArbiterSP, PhysicsShape, S, PhysicsParticle, P, 1);
            AuxiliaryDelete(PhysicsArbiterSS, PhysicsShape, S1, PhysicsShape, S2, 2);
            AuxiliaryDelete(PhysicsArbiterS, PhysicsShape, S, MapFormwork, M, 3);
            AuxiliaryDelete(PhysicsArbiterP, PhysicsParticle, P, MapFormwork, M, 4);
            AuxiliaryDelete(PhysicsArbiterC, PhysicsCircle, C, MapFormwork, M, 5);
            AuxiliaryDelete(PhysicsArbiterCS, PhysicsCircle, C, PhysicsShape, S, 6);
            AuxiliaryDelete(PhysicsArbiterCC, PhysicsCircle, C1, PhysicsCircle, C2, 7);

        default:
            break;
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

        const auto XT_Fun = [this](int T_Num, int Tx)
        {
            bool JZ;
            int SizeD = PhysicsShapeS.size() / Tx;
            int SizeY = PhysicsShapeS.size() % Tx;
            if (T_Num < SizeY)
            {
                SizeY = SizeD * T_Num + T_Num + SizeD + 1;
                SizeD = SizeD * T_Num + T_Num;
            }
            else
            {
                int Y = SizeY;
                SizeY = SizeD * T_Num + Y + SizeD;
                SizeD = SizeD * T_Num + Y;
            }
            for (; SizeD < SizeY; ++SizeD)
            {
                // 静止了，跳过碰撞遍历
                JZ = (PhysicsShapeS[SizeD]->StaticNum > 10);

                // 和地形的碰撞
                if (!JZ) Arbiter(PhysicsShapeS[SizeD], wMapFormwork);

                for (auto o : PhysicsParticleS)
                {
                    if (JZ && o->StaticNum > 10) {
                        continue;
                    }
                    if (PhysicsShapeS[SizeD]->DropCollision(o->pos).Collision)
                    {
                        o->OldPosUpDataBool = false; // 关闭旧位置更新
                        Arbiter(PhysicsShapeS[SizeD], o);
                    }
                    else
                    {
                        ArbiterKey key = ArbiterKey(PhysicsShapeS[SizeD], o);
                        if (CollideGroupS.find(key) != CollideGroupS.end())
                        {
#if ThreadPoolBool
                            mLock.lock();
#endif
                            CollideGroupS.erase(key);
#if ThreadPoolBool
                            mLock.unlock();
#endif
                        }
                    }
                }

                for (auto c : PhysicsCircleS)
                {
                    if (JZ && c->StaticNum > 10) {
                        continue;
                    }
                    if ((PhysicsShapeS[SizeD]->CollisionR + c->radius) < Modulus(PhysicsShapeS[SizeD]->pos - c->pos))
                    {
                        ArbiterKey key = ArbiterKey(c, PhysicsShapeS[SizeD]);
                        if (CollideGroupS.find(key) != CollideGroupS.end())
                        {
#if ThreadPoolBool
                            mLock.lock();
#endif
                            CollideGroupS.erase(key);
#if ThreadPoolBool
                            mLock.unlock();
#endif
                        }
                        continue;
                    }
                    Arbiter(c, PhysicsShapeS[SizeD]);
                }

                // 和其他多边形的碰撞
                for (size_t j = SizeD + 1; j < PhysicsShapeS.size(); ++j)
                {
                    if (JZ && PhysicsShapeS[j]->StaticNum > 10) {
                        continue;
                    }
                    if ((PhysicsShapeS[SizeD]->CollisionR + PhysicsShapeS[j]->CollisionR) < Modulus(PhysicsShapeS[SizeD]->pos - PhysicsShapeS[j]->pos))
                    {
                        ArbiterKey key = ArbiterKey(PhysicsShapeS[SizeD], PhysicsShapeS[j]);
                        if (CollideGroupS.find(key) != CollideGroupS.end())
                        {
#if ThreadPoolBool
                            mLock.lock();
#endif
                            CollideGroupS.erase(key);
#if ThreadPoolBool
                            mLock.unlock();
#endif
                        }
                        continue;
                    }
                    Arbiter(PhysicsShapeS[SizeD], PhysicsShapeS[j]);
                }
            }

            SizeD = PhysicsCircleS.size() / Tx;
            SizeY = PhysicsCircleS.size() % Tx;
            if (T_Num < SizeY)
            {
                SizeY = SizeD * T_Num + T_Num + SizeD + 1;
                SizeD = SizeD * T_Num + T_Num;
            }
            else
            {
                int Y = SizeY;
                SizeY = SizeD * T_Num + Y + SizeD;
                SizeD = SizeD * T_Num + Y;
            }
            for (; SizeD < SizeY; ++SizeD)
            {
                // 静止了，跳过碰撞遍历
                JZ = (PhysicsCircleS[SizeD]->StaticNum > 10);

                // 和地形的碰撞
                if (!JZ) Arbiter(PhysicsCircleS[SizeD], wMapFormwork);

                for (auto o : PhysicsParticleS)
                {
                    if (JZ && o->StaticNum > 10) {
                        continue;
                    }
                    if (PhysicsCircleS[SizeD]->radius > Modulus(PhysicsCircleS[SizeD]->pos - o->pos))
                    {
                        o->OldPosUpDataBool = false; // 关闭旧位置更新
                        Arbiter(PhysicsCircleS[SizeD], o);
                    }
                    else
                    {
                        ArbiterKey key = ArbiterKey(PhysicsCircleS[SizeD], o);
                        if (CollideGroupS.find(key) != CollideGroupS.end())
                        {
#if ThreadPoolBool
                            mLock.lock();
#endif
                            CollideGroupS.erase(key);
#if ThreadPoolBool
                            mLock.unlock();
#endif
                        }
                    }
                }

                for (size_t j = SizeD + 1; j < PhysicsCircleS.size(); ++j)
                {
                    if (JZ && PhysicsCircleS[j]->StaticNum > 10) {
                        continue;
                    }
                    if ((PhysicsCircleS[SizeD]->radius + PhysicsCircleS[j]->radius) < Modulus(PhysicsCircleS[SizeD]->pos - PhysicsCircleS[j]->pos))
                    {
                        ArbiterKey key = ArbiterKey(PhysicsCircleS[SizeD], PhysicsCircleS[j]);
                        if (CollideGroupS.find(key) != CollideGroupS.end())
                        {
#if ThreadPoolBool
                            mLock.lock();
#endif
                            CollideGroupS.erase(key);
#if ThreadPoolBool
                            mLock.unlock();
#endif
                        }
                        continue;
                    }
                    Arbiter(PhysicsCircleS[SizeD], PhysicsCircleS[j]);
                }
            }
        };

#if ThreadPoolBool
        std::vector<std::future<void>> xTn;
        const int xThreadNum = std::thread::hardware_concurrency();
        int SizeD = PhysicsShapeS.size() / xThreadNum;
        int SizeY = PhysicsShapeS.size() % xThreadNum;
        for (size_t i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(XT_Fun, i, xThreadNum));
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

#if ThreadPoolBool
        // 等待任务结束
        for (auto &tf : xTn)
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
        const auto XT_Fun = [this](int T_Num, int Tx)
        {
            int SizeD = PhysicsShapeS.size() / Tx;
            int SizeY = PhysicsShapeS.size() % Tx;
            if (T_Num < SizeY)
            {
                SizeY = SizeD * T_Num + T_Num + SizeD + 1;
                SizeD = SizeD * T_Num + T_Num;
            }
            else
            {
                int Y = SizeY;
                SizeY = SizeD * T_Num + Y + SizeD;
                SizeD = SizeD * T_Num + Y;
            }
            for (; SizeD < SizeY; ++SizeD)
            {
                // 和地形的碰撞
                Arbiter(PhysicsShapeS[SizeD], wMapFormwork);

                for (auto o : PhysicsParticleS)
                {
                    if (PhysicsShapeS[SizeD]->DropCollision(o->pos).Collision)
                    {
                        o->OldPosUpDataBool = false; // 关闭旧位置更新
                        Arbiter(PhysicsShapeS[SizeD], o);
                    }
                    else
                    {
                        ArbiterKey key = ArbiterKey(PhysicsShapeS[SizeD], o);
                        if (CollideGroupS.find(key) != CollideGroupS.end())
                        {
#if ThreadPoolBool
                            mLock.lock();
#endif
                            CollideGroupS.erase(key);
#if ThreadPoolBool
                            mLock.unlock();
#endif
                        }
                    }
                }

                for (auto c : PhysicsCircleS)
                {
                    if ((PhysicsShapeS[SizeD]->CollisionR + c->radius) < Modulus(PhysicsShapeS[SizeD]->pos - c->pos))
                    {
                        ArbiterKey key = ArbiterKey(c, PhysicsShapeS[SizeD]);
                        if (CollideGroupS.find(key) != CollideGroupS.end())
                        {
#if ThreadPoolBool
                            mLock.lock();
#endif
                            CollideGroupS.erase(key);
#if ThreadPoolBool
                            mLock.unlock();
#endif
                        }
                        continue;
                    }
                    Arbiter(c, PhysicsShapeS[SizeD]);
                }

                // 和其他多边形的碰撞
                for (size_t j = SizeD + 1; j < PhysicsShapeS.size(); ++j)
                {
                    if ((PhysicsShapeS[SizeD]->CollisionR + PhysicsShapeS[j]->CollisionR) < Modulus(PhysicsShapeS[SizeD]->pos - PhysicsShapeS[j]->pos))
                    {
                        ArbiterKey key = ArbiterKey(PhysicsShapeS[SizeD], PhysicsShapeS[j]);
                        if (CollideGroupS.find(key) != CollideGroupS.end())
                        {
#if ThreadPoolBool
                            mLock.lock();
#endif
                            CollideGroupS.erase(key);
#if ThreadPoolBool
                            mLock.unlock();
#endif
                        }
                        continue;
                    }
                    Arbiter(PhysicsShapeS[SizeD], PhysicsShapeS[j]);
                }
            }

            SizeD = PhysicsCircleS.size() / Tx;
            SizeY = PhysicsCircleS.size() % Tx;
            if (T_Num < SizeY)
            {
                SizeY = SizeD * T_Num + T_Num + SizeD + 1;
                SizeD = SizeD * T_Num + T_Num;
            }
            else
            {
                int Y = SizeY;
                SizeY = SizeD * T_Num + Y + SizeD;
                SizeD = SizeD * T_Num + Y;
            }
            for (; SizeD < SizeY; ++SizeD)
            {
                // 和地形的碰撞
                Arbiter(PhysicsCircleS[SizeD], wMapFormwork);

                for (auto o : PhysicsParticleS)
                {
                    if (PhysicsCircleS[SizeD]->radius > Modulus(PhysicsCircleS[SizeD]->pos - o->pos))
                    {
                        o->OldPosUpDataBool = false; // 关闭旧位置更新
                        Arbiter(PhysicsCircleS[SizeD], o);
                    }
                    else
                    {
                        ArbiterKey key = ArbiterKey(PhysicsCircleS[SizeD], o);
                        if (CollideGroupS.find(key) != CollideGroupS.end())
                        {
#if ThreadPoolBool
                            mLock.lock();
#endif
                            CollideGroupS.erase(key);
#if ThreadPoolBool
                            mLock.unlock();
#endif
                        }
                    }
                }

                for (size_t j = SizeD + 1; j < PhysicsCircleS.size(); ++j)
                {
                    if ((PhysicsCircleS[SizeD]->radius + PhysicsCircleS[j]->radius) < Modulus(PhysicsCircleS[SizeD]->pos - PhysicsCircleS[j]->pos))
                    {
                        ArbiterKey key = ArbiterKey(PhysicsCircleS[SizeD], PhysicsCircleS[j]);
                        if (CollideGroupS.find(key) != CollideGroupS.end())
                        {
#if ThreadPoolBool
                            mLock.lock();
#endif
                            CollideGroupS.erase(key);
#if ThreadPoolBool
                            mLock.unlock();
#endif
                        }
                        continue;
                    }
                    Arbiter(PhysicsCircleS[SizeD], PhysicsCircleS[j]);
                }
            }
        };

#if ThreadPoolBool
        std::vector<std::future<void>> xTn;
        const int xThreadNum = std::thread::hardware_concurrency();
        int SizeD = PhysicsShapeS.size() / xThreadNum;
        int SizeY = PhysicsShapeS.size() % xThreadNum;
        for (size_t i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(XT_Fun, i, xThreadNum));
        }
#else
        // 碰撞检测
        T_Fun(0, PhysicsShapeS.size());
#endif

        // 处理 点 和 地形的碰撞
        for (auto o : PhysicsParticleS)
        {
            o->OldPos = o->pos;
            Arbiter(o, wMapFormwork);
        }

#if ThreadPoolBool
        // 等待任务结束
        for (auto &tf : xTn)
        {
            tf.wait();
        }
#endif

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
