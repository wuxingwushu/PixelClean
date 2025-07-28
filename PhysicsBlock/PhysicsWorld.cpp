#include "PhysicsWorld.hpp"
#include "BaseCalculate.hpp"
#include "MapStatic.hpp"
#include <algorithm>
#include <map>
#include <iostream>
#include "PhysicsBaseArbiter.hpp"

// 计算线程任务片段
#define ThreadTaskAllot(S, E, Size, Tn, TSize) \
    if (Tn == TSize)                           \
    {                                          \
        S = 0;                                 \
        E = Size;                              \
    }                                          \
    else                                       \
    {                                          \
        S = Size / TSize;                      \
        E = Size % TSize;                      \
        if (Tn < E)                            \
        {                                      \
            E = S * Tn + Tn + S + 1;           \
            S = S * Tn + Tn;                   \
        }                                      \
        else                                   \
        {                                      \
            int Y = E;                         \
            E = S * Tn + Y + S;                \
            S = S * Tn + Y;                    \
        }                                      \
    }
#if ThreadPoolBool
#define Map_Delete(key)                                 \
    if (CollideGroupS.find(key) != CollideGroupS.end()) \
    {                                                   \
        mLock.lock();                                   \
        DeleteCollideGroup.push_back(key);              \
        mLock.unlock();                                 \
    }
#else
#define Map_Delete(key)                                 \
    if (CollideGroupS.find(key) != CollideGroupS.end()) \
    {                                                   \
        DeleteCollideGroup.push_back(key);              \
    }
#endif

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

    PhysicsWorld::PhysicsWorld(Vec2_ gravityAcceleration, const bool wind) : GravityAcceleration(gravityAcceleration), WindBool(wind)
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
        for (auto i : CollideGroupS)
        {
            DeleteArbiter(i.second);
        }
    }

    inline void PhysicsWorld::HandleCollideGroup(BaseArbiter *Ba)
    {
        Ba->ComputeCollide();
        auto i = CollideGroupS.find(Ba->key);
        if (i == CollideGroupS.end())
        {
            if (Ba->numContacts > 0)
            {
#if ThreadPoolBool
                mLock.lock();
#endif
                NewCollideGroup.push_back(Ba);
#if ThreadPoolBool
                mLock.unlock();
#endif
                return;
            }
        }
        else
        {
            if (Ba->numContacts > 0)
            {
                i->second->Update(Ba->contacts, Ba->numContacts);
            }
            else
            {
#if ThreadPoolBool
                mLock.lock();
#endif
                DeleteCollideGroup.push_back(Ba->key);
#if ThreadPoolBool
                mLock.unlock();
#endif
            }
        }
        DeleteArbiter(Ba);
    }

    void PhysicsWorld::PhysicsEmulator(FLOAT_ time)
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
        for (auto o : PhysicsCircleS)
        {
            o->PhysicsSpeed(time, GravityAcceleration);
        }

#define JFRW 0 /** @brief 是否开启均分任务给每个线程 @warning 不知道为什么开启后性能反而下降了，理论上每个线程处理物体间碰撞物体数量差不多 */

        const auto XT_Fun = [this](int T_Num, int Tx)
        {
            bool JZ;
            int SizeD;
            int SizeY;
            ThreadTaskAllot(SizeD, SizeY, PhysicsShapeS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                // 静止了，跳过碰撞遍历
                JZ = (PhysicsShapeS[SizeD]->StaticNum > 10);

                // 和地形的碰撞
                if (!JZ)
                    Arbiter(PhysicsShapeS[SizeD], wMapFormwork);

                for (auto o : PhysicsParticleS)
                {
                    if (JZ && (o->StaticNum > 10))
                    {
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
                        Map_Delete(key);
                    }
                }

                for (auto c : PhysicsCircleS)
                {
                    if (JZ && (c->StaticNum > 10))
                    {
                        continue;
                    }
                    if ((PhysicsShapeS[SizeD]->CollisionR + c->radius) < Modulus(PhysicsShapeS[SizeD]->pos - c->pos))
                    {
                        ArbiterKey key = ArbiterKey(c, PhysicsShapeS[SizeD]);
                        Map_Delete(key);
                        continue;
                    }
                    Arbiter(c, PhysicsShapeS[SizeD]);
                }
#if JFRW
            }

            SizeD = PhysicsShapeS.size() - int(PhysicsShapeS.size() * sqrt((T_Num + 1) / Tx));
            SizeY = PhysicsShapeS.size() - int(PhysicsShapeS.size() * sqrt(T_Num / Tx));
            for (; SizeD < SizeY; ++SizeD)
            {
                // 静止了，跳过碰撞遍历
                JZ = (PhysicsShapeS[SizeD]->StaticNum > 10);
#endif
                // 和其他多边形的碰撞
                for (size_t j = SizeD + 1; j < PhysicsShapeS.size(); ++j)
                {
                    if (JZ && (PhysicsShapeS[j]->StaticNum > 10))
                    {
                        continue;
                    }
                    if ((PhysicsShapeS[SizeD]->CollisionR + PhysicsShapeS[j]->CollisionR) < Modulus(PhysicsShapeS[SizeD]->pos - PhysicsShapeS[j]->pos))
                    {
                        ArbiterKey key = ArbiterKey(PhysicsShapeS[SizeD], PhysicsShapeS[j]);
                        Map_Delete(key);
                        continue;
                    }
                    Arbiter(PhysicsShapeS[SizeD], PhysicsShapeS[j]);
                }
            }

            ThreadTaskAllot(SizeD, SizeY, PhysicsCircleS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                // 静止了，跳过碰撞遍历
                JZ = (PhysicsCircleS[SizeD]->StaticNum > 10);

                // 和地形的碰撞
                if (!JZ)
                    Arbiter(PhysicsCircleS[SizeD], wMapFormwork);

                for (auto o : PhysicsParticleS)
                {
                    if (JZ && (o->StaticNum > 10))
                    {
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
                        Map_Delete(key);
                    }
                }
#if JFRW
            }

            SizeD = PhysicsCircleS.size() - int(PhysicsCircleS.size() * sqrt((T_Num + 1) / Tx));
            SizeY = PhysicsCircleS.size() - int(PhysicsCircleS.size() * sqrt(T_Num / Tx));
            for (; SizeD < SizeY; ++SizeD)
            {
                // 静止了，跳过碰撞遍历
                JZ = (PhysicsCircleS[SizeD]->StaticNum > 10);
#endif
                for (size_t j = SizeD + 1; j < PhysicsCircleS.size(); ++j)
                {
                    if (JZ && (PhysicsCircleS[j]->StaticNum > 10))
                    {
                        continue;
                    }
                    if ((PhysicsCircleS[SizeD]->radius + PhysicsCircleS[j]->radius) < Modulus(PhysicsCircleS[SizeD]->pos - PhysicsCircleS[j]->pos))
                    {
                        ArbiterKey key = ArbiterKey(PhysicsCircleS[SizeD], PhysicsCircleS[j]);
                        Map_Delete(key);
                        continue;
                    }
                    Arbiter(PhysicsCircleS[SizeD], PhysicsCircleS[j]);
                }
            }
        };

#if ThreadPoolBool
        std::vector<std::future<void>> xTn;
        const int xThreadNum = std::thread::hardware_concurrency();
        for (size_t i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(XT_Fun, i, xThreadNum));
        }
#else
        // 碰撞检测
        XT_Fun(1, 1);
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

        FLOAT_ inv_dt = 1.0 / time;

        for (auto J : DeleteCollideGroup)
        {
            CollideGroupS.erase(J);
        }
        for (int i = 0; i < CollideGroupVector.size(); i++)
        {
            for (int j = 0; j < DeleteCollideGroup.size(); j++)
            {
                if (CollideGroupVector[i]->key == DeleteCollideGroup[j])
                {
                    DeleteArbiter(CollideGroupVector[i]);
                    CollideGroupVector[i] = CollideGroupVector[CollideGroupVector.size() - 1];
                    CollideGroupVector.pop_back();
                    --i;
                    DeleteCollideGroup[j] = DeleteCollideGroup[DeleteCollideGroup.size() - 1];
                    DeleteCollideGroup.pop_back();
                    break;
                }
            }
        }
        DeleteCollideGroup.clear();
        for (auto J : NewCollideGroup)
        {
            CollideGroupS.insert({J->key, J});
            CollideGroupVector.push_back(J);
        }
        NewCollideGroup.clear();

#define DANCI 0 & ThreadPoolBool
#if DANCI
        // 预处理
        const auto PreStepXT_Fun = [this, inv_dt](int T_Num, int Tx)
        {
            bool JZ;
            int SizeD;
            int SizeY;
            ThreadTaskAllot(SizeD, SizeY, CollideGroupVector.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                CollideGroupVector[SizeD]->PreStep(inv_dt);
            }
            ThreadTaskAllot(SizeD, SizeY, PhysicsJointS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                PhysicsJointS[SizeD]->PreStep(inv_dt);
            }
            ThreadTaskAllot(SizeD, SizeY, BaseJunctionS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                BaseJunctionS[SizeD]->PreStep(inv_dt);
            }
        };
        xTn.clear();
        for (size_t i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(PreStepXT_Fun, i, xThreadNum));
        }
        // 等待任务结束
        for (auto &tf : xTn)
        {
            tf.wait();
        }
#else
        // 预处理
        for (const auto i : CollideGroupVector)
        {
            i->PreStep(inv_dt);
        }
        for (auto J : PhysicsJointS)
        {
            J->PreStep(inv_dt);
        }
        for (auto J : BaseJunctionS)
        {
            J->PreStep(inv_dt);
        }
#endif

#if 1 & ThreadPoolBool
        const auto ApplyImpulseXT_Fun = [this](int T_Num, int Tx)
        {
            bool JZ;
            int SizeD;
            int SizeY;
            ThreadTaskAllot(SizeD, SizeY, CollideGroupVector.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                CollideGroupVector[SizeD]->ApplyImpulse();
            }
            ThreadTaskAllot(SizeD, SizeY, PhysicsJointS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                PhysicsJointS[SizeD]->ApplyImpulse();
            }
            ThreadTaskAllot(SizeD, SizeY, BaseJunctionS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                BaseJunctionS[SizeD]->ApplyImpulse();
            }
        };
        for (size_t di = 0; di < 10; ++di)
        {
            xTn.clear();
            for (size_t i = 0; i < xThreadNum; ++i)
            {
                xTn.push_back(mThreadPool.enqueue(ApplyImpulseXT_Fun, i, xThreadNum));
            }
            // 等待任务结束
            for (auto &tf : xTn)
            {
                tf.wait();
            }
        }
#else
        // 迭代结果
        for (size_t i = 0; i < 10; ++i)
        {
            for (auto kv : CollideGroupVector)
            {
                kv->ApplyImpulse();
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

#endif

#if DANCI
        const auto PhysicsPosXT_Fun = [this, time](int T_Num, int Tx)
        {
            bool JZ;
            int SizeD;
            int SizeY;
            ThreadTaskAllot(SizeD, SizeY, PhysicsShapeS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                PhysicsShapeS[SizeD]->PhysicsPos(time, GravityAcceleration);
            }
            ThreadTaskAllot(SizeD, SizeY, PhysicsParticleS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                if (wMapFormwork->FMGetCollide(PhysicsParticleS[SizeD]->pos))
                {
                    PhysicsParticleS[SizeD]->OldPosUpDataBool = false; // 关闭旧位置更新
                }
                PhysicsParticleS[SizeD]->PhysicsPos(time, GravityAcceleration);
            }
            ThreadTaskAllot(SizeD, SizeY, PhysicsCircleS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                PhysicsCircleS[SizeD]->PhysicsPos(time, GravityAcceleration);
            }
        };
        xTn.clear();
        for (size_t i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(PhysicsPosXT_Fun, i, xThreadNum));
        }
        // 等待任务结束
        for (auto &tf : xTn)
        {
            tf.wait();
        }
#else
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
#endif
    }

    void PhysicsWorld::PhysicsInformationUpdate()
    {
        // 碰撞检测
        const auto XT_Fun = [this](int T_Num, int Tx)
        {
            int SizeD;
            int SizeY;
            ThreadTaskAllot(SizeD, SizeY, PhysicsShapeS.size(), T_Num, Tx);
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
                        Map_Delete(key);
                    }
                }

                for (auto c : PhysicsCircleS)
                {
                    if ((PhysicsShapeS[SizeD]->CollisionR + c->radius) < Modulus(PhysicsShapeS[SizeD]->pos - c->pos))
                    {
                        ArbiterKey key = ArbiterKey(c, PhysicsShapeS[SizeD]);
                        Map_Delete(key);
                        continue;
                    }
                    Arbiter(c, PhysicsShapeS[SizeD]);
                }
#if JFRW
            }

            SizeD = PhysicsShapeS.size() - int(PhysicsShapeS.size() * sqrt((T_Num + 1) / Tx));
            SizeY = PhysicsShapeS.size() - int(PhysicsShapeS.size() * sqrt(T_Num / Tx));
            for (; SizeD < SizeY; ++SizeD)
            {
#endif
                // 和其他多边形的碰撞
                for (size_t j = SizeD + 1; j < PhysicsShapeS.size(); ++j)
                {
                    if ((PhysicsShapeS[SizeD]->CollisionR + PhysicsShapeS[j]->CollisionR) < Modulus(PhysicsShapeS[SizeD]->pos - PhysicsShapeS[j]->pos))
                    {
                        ArbiterKey key = ArbiterKey(PhysicsShapeS[SizeD], PhysicsShapeS[j]);
                        Map_Delete(key);
                        continue;
                    }
                    Arbiter(PhysicsShapeS[SizeD], PhysicsShapeS[j]);
                }
            }

            ThreadTaskAllot(SizeD, SizeY, PhysicsCircleS.size(), T_Num, Tx);
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
                        Map_Delete(key);
                    }
                }
#if JFRW
            }

            SizeD = PhysicsCircleS.size() - int(PhysicsCircleS.size() * sqrt((T_Num + 1) / Tx));
            SizeY = PhysicsCircleS.size() - int(PhysicsCircleS.size() * sqrt(T_Num / Tx));
            for (; SizeD < SizeY; ++SizeD)
            {
#endif
                for (size_t j = SizeD + 1; j < PhysicsCircleS.size(); ++j)
                {
                    if ((PhysicsCircleS[SizeD]->radius + PhysicsCircleS[j]->radius) < Modulus(PhysicsCircleS[SizeD]->pos - PhysicsCircleS[j]->pos))
                    {
                        ArbiterKey key = ArbiterKey(PhysicsCircleS[SizeD], PhysicsCircleS[j]);
                        Map_Delete(key);
                        continue;
                    }
                    Arbiter(PhysicsCircleS[SizeD], PhysicsCircleS[j]);
                }
            }
        };

#if ThreadPoolBool
        std::vector<std::future<void>> xTn;
        const int xThreadNum = std::thread::hardware_concurrency() * 2;
        for (size_t i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(XT_Fun, i, xThreadNum));
        }
#else
        // 碰撞检测
        XT_Fun(1, 1);
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
#if ConcurrentHash_map_
        // 1. 获取锁定表（独占访问）
        auto locked_table = CollideGroupS.lock_table();
        // 2. 使用迭代器遍历
        for (auto kv = locked_table.begin(); kv != locked_table.end(); ++kv)
        {
            kv->second->PreStep();
        }
#else
        for (auto kv : CollideGroupS)
        {
            kv.second->PreStep();
        }
#endif
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
        GridWind = new Vec2_[GridWindSize.x * GridWindSize.y];
        for (size_t i = 0; i < (GridWindSize.x * GridWindSize.y); i++)
        {
            GridWind[i] = Vec2_{0};
        }
    }

    PhysicsFormwork *PhysicsWorld::Get(Vec2_ pos)
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

    FLOAT_ PhysicsWorld::GetWorldEnergy()
    {
        FLOAT_ Energy = 0;
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
