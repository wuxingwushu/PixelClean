#include "PhysicsWorld.hpp"
#include "BaseCalculate.hpp"
#include "MapStatic.hpp"
#include "MapDynamic.hpp"
#include <algorithm>
#include <map>
#include <iostream>
#include "PhysicsBaseArbiter.hpp"
#include "PhysicsBaseCollide.hpp"

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
    AuxiliaryArbiter(PhysicsArbiterLC, PhysicsLine, L, PhysicsCircle, C, 8);
    AuxiliaryArbiter(PhysicsArbiterLS, PhysicsLine, L, PhysicsShape, S, 9);
    AuxiliaryArbiter(PhysicsArbiterLP, PhysicsLine, L, PhysicsParticle, P, 10);
    AuxiliaryArbiter(PhysicsArbiterL, PhysicsLine, L, MapFormwork, M, 11);

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
            AuxiliaryDelete(PhysicsArbiterLC, PhysicsLine, L, PhysicsCircle, C, 8);
            AuxiliaryDelete(PhysicsArbiterLS, PhysicsLine, L, PhysicsShape, S, 9);
            AuxiliaryDelete(PhysicsArbiterLP, PhysicsLine, L, PhysicsParticle, P, 10);
            AuxiliaryDelete(PhysicsArbiterL, PhysicsLine, L, MapFormwork, M, 11);

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
        // 等待线程结束
        for (auto &tf : xTn)
        {
            tf.wait();
        }

        if (GridWind != nullptr)
        {
            delete GridWind;
        }
        if (wMapFormwork != nullptr)
        {
            if (wMapFormwork->FMGetType() == _MapStatic)
            {
                delete (MapStatic *)wMapFormwork;
            }
            else if (wMapFormwork->FMGetType() == _MapDynamic)
            {
                delete (MapDynamic *)wMapFormwork;
            }
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
        // 物理圆
        for (auto i : PhysicsCircleS)
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
        // 物理线
        for (auto i : PhysicsLineS)
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
#if ThreadPoolBool

        const int xThreadNum = std::thread::hardware_concurrency();

        // 等待 判断物体间的碰撞 的 任务结束
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
#endif

        // 更新网格
        mGridSearch.UpData();

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

                std::vector<PhysicsBlock::PhysicsFormwork *> SearchV = mGridSearch.Get(PhysicsShapeS[SizeD]->pos, PhysicsShapeS[SizeD]->radius);
                for (auto i : SearchV)
                {
                    switch (i->PFGetType())
                    {
                    case PhysicsObjectEnum::circle:
                        if (JZ && (((PhysicsCircle *)i)->StaticNum > 10))
                        {
                            break;
                        }
                        if (!CollideAABB(PhysicsShapeS[SizeD], ((PhysicsCircle *)i)))
                        {
                            ArbiterKey key = ArbiterKey(((PhysicsCircle *)i), PhysicsShapeS[SizeD]);
                            Map_Delete(key);
                            break;
                        }
                        Arbiter(((PhysicsCircle *)i), PhysicsShapeS[SizeD]);
                        break;
                    case PhysicsObjectEnum::particle:
                        if (JZ && (((PhysicsParticle *)i)->StaticNum > 10))
                        {
                            break;
                        }
                        if (PhysicsShapeS[SizeD]->DropCollision(((PhysicsParticle *)i)->pos).Collision)
                        {
                            ((PhysicsParticle *)i)->OldPosUpDataBool = false; // 关闭旧位置更新
                            Arbiter(PhysicsShapeS[SizeD], ((PhysicsParticle *)i));
                        }
                        else
                        {
                            ArbiterKey key = ArbiterKey(PhysicsShapeS[SizeD], ((PhysicsParticle *)i));
                            Map_Delete(key);
                        }
                        break;
                    case PhysicsObjectEnum::shape:
                        if (PhysicsShapeS[SizeD] == ((PhysicsShape *)i))
                        {
                            break;
                        }
                        if (JZ && (((PhysicsShape *)i)->StaticNum > 10))
                        {
                            break;
                        }
                        if (!CollideAABB(PhysicsShapeS[SizeD], ((PhysicsShape *)i)))
                        {
                            ArbiterKey key = ArbiterKey(((PhysicsShape *)i), PhysicsShapeS[SizeD]);
                            Map_Delete(key);
                            break;
                        }
                        Arbiter(((PhysicsShape *)i), PhysicsShapeS[SizeD]);
                        break;

                    default:
                        break;
                    }
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

                std::vector<PhysicsBlock::PhysicsFormwork *> SearchV = mGridSearch.Get(PhysicsCircleS[SizeD]->pos, PhysicsCircleS[SizeD]->radius);
                for (auto i : SearchV)
                {
                    switch (i->PFGetType())
                    {
                    case PhysicsObjectEnum::circle:
                        if (PhysicsCircleS[SizeD] == ((PhysicsCircle *)i))
                        {
                            break;
                        }
                        if (JZ && (((PhysicsCircle *)i)->StaticNum > 10))
                        {
                            break;
                        }
                        if (!CollideAABB(PhysicsCircleS[SizeD], ((PhysicsCircle *)i)))
                        {
                            ArbiterKey key = ArbiterKey(((PhysicsCircle *)i), PhysicsCircleS[SizeD]);
                            Map_Delete(key);
                            break;
                        }
                        Arbiter(((PhysicsCircle *)i), PhysicsCircleS[SizeD]);
                        break;
                    case PhysicsObjectEnum::particle:
                        if (JZ && (((PhysicsParticle *)i)->StaticNum > 10))
                        {
                            break;
                        }
                        if (CollideAABB(PhysicsCircleS[SizeD], ((PhysicsParticle *)i)))
                        {
                            ((PhysicsParticle *)i)->OldPosUpDataBool = false; // 关闭旧位置更新
                            Arbiter(PhysicsCircleS[SizeD], ((PhysicsParticle *)i));
                        }
                        else
                        {
                            ArbiterKey key = ArbiterKey(PhysicsCircleS[SizeD], ((PhysicsParticle *)i));
                            Map_Delete(key);
                        }
                        break;

                    default:
                        break;
                    }
                }
            }

            ThreadTaskAllot(SizeD, SizeY, PhysicsLineS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                // 静止了，跳过碰撞遍历
                JZ = (PhysicsLineS[SizeD]->StaticNum > 10);

                // 和地形的碰撞
                if (!JZ)
                    Arbiter(PhysicsLineS[SizeD], wMapFormwork);

                std::vector<PhysicsBlock::PhysicsFormwork *> SearchV = mGridSearch.Get(PhysicsLineS[SizeD]->pos, PhysicsLineS[SizeD]->radius);
                for (auto i : SearchV)
                {
                    switch (i->PFGetType())
                    {
                    case PhysicsObjectEnum::circle:
                        if (JZ && (((PhysicsCircle *)i)->StaticNum > 10))
                        {
                            break;
                        }
                        if (!CollideAABB(PhysicsLineS[SizeD], ((PhysicsCircle *)i)))
                        {
                            ArbiterKey key = ArbiterKey(((PhysicsCircle *)i), PhysicsLineS[SizeD]);
                            Map_Delete(key);
                            break;
                        }
                        Arbiter(PhysicsLineS[SizeD], ((PhysicsCircle *)i));
                        break;
                    case PhysicsObjectEnum::particle:
                        if (JZ && (((PhysicsParticle *)i)->StaticNum > 10))
                        {
                            break;
                        }
                        Arbiter(PhysicsLineS[SizeD], ((PhysicsParticle *)i));
                        break;
                    case PhysicsObjectEnum::shape:
                        if (JZ && (((PhysicsShape *)i)->StaticNum > 10))
                        {
                            break;
                        }
                        if (!CollideAABB(PhysicsLineS[SizeD], ((PhysicsShape *)i)))
                        {
                            ArbiterKey key = ArbiterKey(((PhysicsShape *)i), PhysicsLineS[SizeD]);
                            Map_Delete(key);
                            break;
                        }
                        Arbiter(PhysicsLineS[SizeD], ((PhysicsShape *)i));
                        break;

                    default:
                        break;
                    }
                }
            }

            ThreadTaskAllot(SizeD, SizeY, PhysicsParticleS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                // 静止了，跳过碰撞遍历
                if (PhysicsParticleS[SizeD]->StaticNum > 10)
                {
                    continue;
                }

                Arbiter(PhysicsParticleS[SizeD], wMapFormwork);
            }
        };

        FLOAT_ inv_dt = 1.0 / time;

        for (auto &D : DeleteCollideGroup)
        {
            if (CollideGroupS.find(D) == CollideGroupS.end())
            {
                continue;
            }
            CollideGroupS.erase(D);
            for (int i = 0; i < CollideGroupVector.size(); ++i)
            {
                if (CollideGroupVector[i]->key == D)
                {
                    DeleteArbiter(CollideGroupVector[i]);
                    CollideGroupVector[i] = CollideGroupVector[CollideGroupVector.size() - 1];
                    CollideGroupVector.pop_back();
                    break;
                }
            }
        }
        DeleteCollideGroup.clear();
        for (auto &J : NewCollideGroup)
        {
            if (CollideGroupS.find(J->key) == CollideGroupS.end())
            {
                CollideGroupS.insert({J->key, J});
                CollideGroupVector.push_back(J);
            }
            else
            {
                DeleteArbiter(J);
            }
        }
        NewCollideGroup.clear();

#define Definite 0 // 是否需要确定性(暂时没法保证确定性，没有对解算的前后顺序进行排序)

// 这里的预处理，存在冲量影响（速度，角速度），所以使用多线程没法收敛静止物体，且增加不确定性。
#if (Definite != 1) & ThreadPoolBool & 0
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
        for (size_t i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(PreStepXT_Fun, i, xThreadNum));
        }
        // 等待 预处理 任务结束
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
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

// 这里使用多线程虽然会增加不确定性，但是现在我暂时不需求确定性。所以舍弃也无所谓
#if (Definite != 1) & ThreadPoolBool
        // 迭代结果
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
            for (size_t i = 0; i < xThreadNum; ++i)
            {
                xTn.push_back(mThreadPool.enqueue(ApplyImpulseXT_Fun, i, xThreadNum));
            }
            // 等待 迭代结果 任务结束
            for (auto &tf : xTn)
            {
                tf.wait();
            }
            xTn.clear();
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

// 这个多线程不影响结果
#if 1 & ThreadPoolBool
        // 移动
        const auto PhysicsPosXT_Fun = [this, time](int T_Num, int Tx)
        {
            bool JZ;
            int SizeD;
            int SizeY;
            ThreadTaskAllot(SizeD, SizeY, PhysicsShapeS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                PhysicsShapeS[SizeD]->PhysicsPos(time, GravityAcceleration);
                PhysicsShapeS[SizeD]->PhysicsSpeed(time, GravityAcceleration);
            }
            ThreadTaskAllot(SizeD, SizeY, PhysicsParticleS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                PhysicsParticleS[SizeD]->PhysicsPos(time, GravityAcceleration);
                PhysicsParticleS[SizeD]->PhysicsSpeed(time, GravityAcceleration);
            }
            ThreadTaskAllot(SizeD, SizeY, PhysicsCircleS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                PhysicsCircleS[SizeD]->PhysicsPos(time, GravityAcceleration);
                PhysicsCircleS[SizeD]->PhysicsSpeed(time, GravityAcceleration);
            }
            ThreadTaskAllot(SizeD, SizeY, PhysicsLineS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                PhysicsLineS[SizeD]->PhysicsPos(time, GravityAcceleration);
                PhysicsLineS[SizeD]->PhysicsSpeed(time, GravityAcceleration);
            }
        };
        for (size_t i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(PhysicsPosXT_Fun, i, xThreadNum));
        }
        // 等待 移动 任务结束
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
#else
        // 移动 & 外力改变
        for (auto i : PhysicsShapeS)
        {
            i->PhysicsPos(time, GravityAcceleration);
            i->PhysicsSpeed(time, GravityAcceleration);
        }
        for (auto i : PhysicsParticleS)
        {
            i->PhysicsPos(time, GravityAcceleration);
            i->PhysicsSpeed(time, GravityAcceleration);
        }
        for (auto i : PhysicsCircleS)
        {
            i->PhysicsPos(time, GravityAcceleration);
            i->PhysicsSpeed(time, GravityAcceleration);
        }
        for (auto i : PhysicsLineS)
        {
            i->PhysicsPos(time, GravityAcceleration);
            i->PhysicsSpeed(time, GravityAcceleration);
        }
#endif

#if ThreadPoolBool
        // 判断物体间的碰撞（不影响位置，所以可以不用强制等待完成）
        for (size_t i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(XT_Fun, i, xThreadNum));
        }
#else
        // 碰撞检测
        XT_Fun(1, 1);
#endif
    }

    void PhysicsWorld::PhysicsInformationUpdate()
    {
#if ThreadPoolBool
        const int xThreadNum = std::thread::hardware_concurrency();

        // 等待 判断物体间的碰撞 任务结束
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
#endif

        // 更新网格
        mGridSearch.UpData();

        // 碰撞检测
        const auto XT_Fun = [this](int T_Num, int Tx)
        {
            int SizeD;
            int SizeY;
            ThreadTaskAllot(SizeD, SizeY, PhysicsShapeS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                Arbiter(PhysicsShapeS[SizeD], wMapFormwork);

                std::vector<PhysicsBlock::PhysicsFormwork *> SearchV = mGridSearch.Get(PhysicsShapeS[SizeD]->pos, PhysicsShapeS[SizeD]->radius);
                for (auto i : SearchV)
                {
                    switch (i->PFGetType())
                    {
                    case PhysicsObjectEnum::circle:
                        if (!CollideAABB(PhysicsShapeS[SizeD], ((PhysicsCircle *)i)))
                        {
                            ArbiterKey key = ArbiterKey(((PhysicsCircle *)i), PhysicsShapeS[SizeD]);
                            Map_Delete(key);
                            break;
                        }
                        Arbiter(((PhysicsCircle *)i), PhysicsShapeS[SizeD]);
                        break;
                    case PhysicsObjectEnum::particle:
                        if (PhysicsShapeS[SizeD]->DropCollision(((PhysicsParticle *)i)->pos).Collision)
                        {
                            ((PhysicsParticle *)i)->OldPosUpDataBool = false; // 关闭旧位置更新
                            Arbiter(PhysicsShapeS[SizeD], ((PhysicsParticle *)i));
                        }
                        else
                        {
                            ArbiterKey key = ArbiterKey(PhysicsShapeS[SizeD], ((PhysicsParticle *)i));
                            Map_Delete(key);
                        }
                        break;
                    case PhysicsObjectEnum::shape:
                        if (PhysicsShapeS[SizeD] == ((PhysicsShape *)i))
                        {
                            break;
                        }
                        if (!CollideAABB(PhysicsShapeS[SizeD], ((PhysicsShape *)i)))
                        {
                            ArbiterKey key = ArbiterKey(((PhysicsShape *)i), PhysicsShapeS[SizeD]);
                            Map_Delete(key);
                            break;
                        }
                        Arbiter(((PhysicsShape *)i), PhysicsShapeS[SizeD]);
                        break;

                    default:
                        break;
                    }
                }
            }

            ThreadTaskAllot(SizeD, SizeY, PhysicsCircleS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                Arbiter(PhysicsCircleS[SizeD], wMapFormwork);

                std::vector<PhysicsBlock::PhysicsFormwork *> SearchV = mGridSearch.Get(PhysicsCircleS[SizeD]->pos, PhysicsCircleS[SizeD]->radius);
                for (auto i : SearchV)
                {
                    switch (i->PFGetType())
                    {
                    case PhysicsObjectEnum::circle:
                        if (PhysicsCircleS[SizeD] == ((PhysicsCircle *)i))
                        {
                            break;
                        }
                        if (!CollideAABB(PhysicsCircleS[SizeD], ((PhysicsCircle *)i)))
                        {
                            ArbiterKey key = ArbiterKey(((PhysicsCircle *)i), PhysicsCircleS[SizeD]);
                            Map_Delete(key);
                            break;
                        }
                        Arbiter(((PhysicsCircle *)i), PhysicsCircleS[SizeD]);
                        break;
                    case PhysicsObjectEnum::particle:
                        if (CollideAABB(PhysicsCircleS[SizeD], ((PhysicsParticle *)i)))
                        {
                            ((PhysicsParticle *)i)->OldPosUpDataBool = false; // 关闭旧位置更新
                            Arbiter(PhysicsCircleS[SizeD], ((PhysicsParticle *)i));
                        }
                        else
                        {
                            ArbiterKey key = ArbiterKey(PhysicsCircleS[SizeD], ((PhysicsParticle *)i));
                            Map_Delete(key);
                        }
                        break;

                    default:
                        break;
                    }
                }
            }

            ThreadTaskAllot(SizeD, SizeY, PhysicsLineS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                Arbiter(PhysicsLineS[SizeD], wMapFormwork);

                std::vector<PhysicsBlock::PhysicsFormwork *> SearchV = mGridSearch.Get(PhysicsLineS[SizeD]->pos, PhysicsLineS[SizeD]->radius);
                for (auto i : SearchV)
                {
                    switch (i->PFGetType())
                    {
                    case PhysicsObjectEnum::circle:
                        if (!CollideAABB(PhysicsLineS[SizeD], ((PhysicsCircle *)i)))
                        {
                            ArbiterKey key = ArbiterKey(((PhysicsCircle *)i), PhysicsLineS[SizeD]);
                            Map_Delete(key);
                            break;
                        }
                        Arbiter(PhysicsLineS[SizeD], ((PhysicsCircle *)i));
                        break;
                    case PhysicsObjectEnum::particle:
                        Arbiter(PhysicsLineS[SizeD], ((PhysicsParticle *)i));
                        break;
                    case PhysicsObjectEnum::shape:
                        if (!CollideAABB(PhysicsLineS[SizeD], ((PhysicsShape *)i)))
                        {
                            ArbiterKey key = ArbiterKey(((PhysicsShape *)i), PhysicsLineS[SizeD]);
                            Map_Delete(key);
                            break;
                        }
                        Arbiter(PhysicsLineS[SizeD], ((PhysicsShape *)i));
                        break;

                    default:
                        break;
                    }
                }
            }

            ThreadTaskAllot(SizeD, SizeY, PhysicsParticleS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                // 处理 点 与 地形 的碰撞
                Arbiter(PhysicsParticleS[SizeD], wMapFormwork);
            }
        };

        for (auto &D : DeleteCollideGroup)
        {
            if (CollideGroupS.find(D) == CollideGroupS.end())
            {
                continue;
            }
            CollideGroupS.erase(D);
            for (int i = 0; i < CollideGroupVector.size(); ++i)
            {
                if (CollideGroupVector[i]->key == D)
                {
                    DeleteArbiter(CollideGroupVector[i]);
                    CollideGroupVector[i] = CollideGroupVector[CollideGroupVector.size() - 1];
                    CollideGroupVector.pop_back();
                    break;
                }
            }
        }
        DeleteCollideGroup.clear();
        for (auto &J : NewCollideGroup)
        {
            if (CollideGroupS.find(J->key) == CollideGroupS.end())
            {
                CollideGroupS.insert({J->key, J});
                CollideGroupVector.push_back(J);
            }
            else
            {
                DeleteArbiter(J);
            }
        }
        NewCollideGroup.clear();

#if 1
        // 预处理
        const auto PreStepXT_Fun = [this](int T_Num, int Tx)
        {
            bool JZ;
            int SizeD;
            int SizeY;
            ThreadTaskAllot(SizeD, SizeY, CollideGroupVector.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                CollideGroupVector[SizeD]->PreStep();
            }
        };
        for (size_t i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(PreStepXT_Fun, i, xThreadNum));
        }
        // 等待 预处理 任务结束
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
#else
        // 预处理
        for (const auto i : CollideGroupVector)
        {
            i->PreStep();
        }
#endif

#if ThreadPoolBool
        // 判断物体间的碰撞（不影响位置，所以可以不用强制等待完成）
        for (size_t i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(XT_Fun, i, xThreadNum));
        }
#else
        // 碰撞检测
        XT_Fun(1, 1);
#endif
    }

    void PhysicsWorld::SetMapFormwork(MapFormwork *MapFormwork_)
    {
        GridWindSize = MapFormwork_->FMGetMapSize();
        mGridSearch.SetMapRange(std::max(GridWindSize.x, GridWindSize.y));

        wMapFormwork = MapFormwork_;
        if (!WindBool)
        {
            return;
        }
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
        std::vector<PhysicsBlock::PhysicsFormwork *> SearchV = mGridSearch.Get(pos, 1);
        for (auto i : SearchV)
        {
            switch (i->PFGetType())
            {
            case PhysicsObjectEnum::circle:
                if (Modulus(((PhysicsCircle *)i)->pos - pos) < ((PhysicsCircle *)i)->radius)
                    return ((PhysicsCircle *)i);
                break;
            case PhysicsObjectEnum::particle:
                if (Modulus(((PhysicsParticle *)i)->pos - pos) < 0.25) // 点击位置距离点位置小于 0.25， 就判断选择 点
                    return ((PhysicsParticle *)i);
                break;
            case PhysicsObjectEnum::shape:
                if ((Modulus(((PhysicsShape *)i)->pos - pos) < ((PhysicsShape *)i)->PFGetCollisionR()) && (((PhysicsShape *)i)->DropCollision(pos).Collision))
                    return ((PhysicsShape *)i);
                break;

            default:
                break;
            }
        }
        Vec2_ Drop;
        for (auto i : PhysicsLineS)
        {
            Drop = DropUptoLineShortesIntersect(i->pos + vec2angle({i->radius, 0}, i->angle), i->pos - vec2angle({i->radius, 0}, i->angle), pos);
            if (Modulus(Drop - pos) < 0.25)
                return ((PhysicsLine *)i);
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
        for (auto i : PhysicsCircleS)
        {
            Energy += i->mass * ModulusLength(i->speed);
            Energy += i->MomentInertia * i->angleSpeed * i->angleSpeed;
        }
        return Energy / 2;
    }
#if PhysicsBlock_Serialization
    PhysicsWorld::PhysicsWorld(const nlohmann::json_abi_v3_12_0::basic_json<> &data) : WindBool(data["WindBool"])
    {
        JsonContrarySerialization(data);
    }

    void PhysicsWorld::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        data["WindBool"] = WindBool;
        SerializationVec2(data, Wind);
        SerializationVec2(data, GravityAcceleration);
        SerializationVec2(data, GridWindSize);
        unsigned int dataIndex = 0;
        if (WindBool)
        {
            nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["GridWind"];
            dataArray = dataArray.array();
            for (size_t i = 0; i < (GridWindSize.x * GridWindSize.y); ++i)
            {
                SerializationVec2(dataArray[i], GridWind[i]);
            }
        }
        if (wMapFormwork)
        {
            switch (wMapFormwork->FMGetType())
            {
            case PhysicsObjectEnum::_MapStatic:
                data["wMapFormwork"]["Type"] = PhysicsObjectEnum::_MapStatic;
                ((MapStatic *)wMapFormwork)->JsonSerialization(data["wMapFormwork"]);
                break;

            default:
                break;
            }
        }
        if (PhysicsShapeS.size() != 0)
        {
            dataIndex = 0;
            nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["PhysicsShapeS"];
            dataArray = dataArray.array();
            for (auto j : PhysicsShapeS)
            {
                j->JsonSerialization(dataArray[dataIndex]);
                ++dataIndex;
            }
        }
        if (PhysicsParticleS.size() != 0)
        {
            dataIndex = 0;
            nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["PhysicsParticleS"];
            dataArray = dataArray.array();
            for (auto j : PhysicsParticleS)
            {
                j->JsonSerialization(dataArray[dataIndex]);
                ++dataIndex;
            }
        }
        if (PhysicsCircleS.size() != 0)
        {
            dataIndex = 0;
            nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["PhysicsCircleS"];
            dataArray = dataArray.array();
            for (auto j : PhysicsCircleS)
            {
                j->JsonSerialization(dataArray[dataIndex]);
                ++dataIndex;
            }
        }
        if (PhysicsLineS.size() != 0)
        {
            dataIndex = 0;
            nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["PhysicsLineS"];
            dataArray = dataArray.array();
            for (auto j : PhysicsLineS)
            {
                j->JsonSerialization(dataArray[dataIndex]);
                ++dataIndex;
            }
        }
        if (PhysicsJointS.size() != 0)
        {
            dataIndex = 0;
            nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["PhysicsJointS"];
            dataArray = dataArray.array();
            for (auto j : PhysicsJointS)
            {
                dataArray[dataIndex]["body1"] = GetPtrIndex(j->body1);
                dataArray[dataIndex]["body1Type"] = j->body1->PFGetType();
                dataArray[dataIndex]["body2"] = GetPtrIndex(j->body2);
                dataArray[dataIndex]["body2Type"] = j->body2->PFGetType();
                j->JsonSerialization(dataArray[dataIndex]);
                ++dataIndex;
            }
        }
        if (BaseJunctionS.size() != 0)
        {
            dataIndex = 0;
            nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["BaseJunctionS"];
            dataArray = dataArray.array();
            for (auto j : BaseJunctionS)
            {
                dataArray[dataIndex]["ObjectType"] = j->ObjectType();
                switch (j->ObjectType())
                {
                case CordObjectType::JunctionAA:
                    dataArray[dataIndex]["body1"] = GetPtrIndex(((PhysicsJunctionSS *)j)->mParticle1);
                    dataArray[dataIndex]["body1Type"] = ((PhysicsJunctionSS *)j)->mParticle1->PFGetType();
                    dataArray[dataIndex]["body2"] = GetPtrIndex(((PhysicsJunctionSS *)j)->mParticle2);
                    dataArray[dataIndex]["body2Type"] = ((PhysicsJunctionSS *)j)->mParticle2->PFGetType();
                    break;
                case CordObjectType::JunctionA:
                    dataArray[dataIndex]["body1"] = GetPtrIndex(((PhysicsJunctionS *)j)->mParticle);
                    dataArray[dataIndex]["body1Type"] = ((PhysicsJunctionS *)j)->mParticle->PFGetType();
                    break;
                case CordObjectType::JunctionP:
                    dataArray[dataIndex]["body1"] = GetPtrIndex(((PhysicsJunctionP *)j)->mParticle);
                    dataArray[dataIndex]["body1Type"] = ((PhysicsJunctionP *)j)->mParticle->PFGetType();
                    break;
                case CordObjectType::JunctionPP:
                    dataArray[dataIndex]["body1"] = GetPtrIndex(((PhysicsJunctionPP *)j)->mParticle1);
                    dataArray[dataIndex]["body1Type"] = ((PhysicsJunctionPP *)j)->mParticle1->PFGetType();
                    dataArray[dataIndex]["body2"] = GetPtrIndex(((PhysicsJunctionPP *)j)->mParticle2);
                    dataArray[dataIndex]["body2Type"] = ((PhysicsJunctionPP *)j)->mParticle2->PFGetType();
                    break;

                default:
                    break;
                }
                j->JsonSerialization(dataArray[dataIndex]);
                ++dataIndex;
            }
        }
        if (CollideGroupVector.size() != 0)
        {
            dataIndex = 0;
            nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["CollideGroupVector"];
            dataArray = dataArray.array();
            for (auto j : CollideGroupVector)
            {
                dataArray[dataIndex]["type"] = j->GetArbiterType();
                if (j->GetArbiterType() > PhysicsArbiterType::ArbiterL)
                {

                    dataArray[dataIndex]["body1"] = GetPtrIndex(((PhysicsFormwork *)j->key.object1));
                    dataArray[dataIndex]["body1Type"] = ((PhysicsFormwork *)j->key.object1)->PFGetType();
                    dataArray[dataIndex]["body2"] = GetPtrIndex(((PhysicsFormwork *)j->key.object2));
                    dataArray[dataIndex]["body2Type"] = ((PhysicsFormwork *)j->key.object2)->PFGetType();
                }
                else
                {
                    if (j->key.object1 == wMapFormwork)
                    {
                        dataArray[dataIndex]["body1"] = GetPtrIndex(((PhysicsFormwork *)j->key.object2));
                        dataArray[dataIndex]["body1Type"] = ((PhysicsFormwork *)j->key.object2)->PFGetType();
                    }
                    else
                    {
                        dataArray[dataIndex]["body1"] = GetPtrIndex(((PhysicsFormwork *)j->key.object1));
                        dataArray[dataIndex]["body1Type"] = ((PhysicsFormwork *)j->key.object1)->PFGetType();
                    }
                }
                j->JsonSerialization(dataArray[dataIndex]);
                ++dataIndex;
            }
        }
    }

    void PhysicsWorld::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        // WindBool = data["WindBool"];
        ContrarySerializationVec2(data, Wind);
        ContrarySerializationVec2(data, GravityAcceleration);
        ContrarySerializationVec2(data, GridWindSize);
        if (WindBool)
        {
            GridWind = new Vec2_[data["GridWind"].size()];
            for (size_t i = 0; i < (GridWindSize.x * GridWindSize.y); ++i)
            {
                ContrarySerializationVec2(data["GridWind"][i], GridWind[i]);
            }
        }
        if (data.find("wMapFormwork") != data.end())
        {
            switch (PhysicsObjectEnum(data["wMapFormwork"]["Type"]))
            {
            case PhysicsObjectEnum::_MapStatic:
                wMapFormwork = new MapStatic(data["wMapFormwork"]);
                break;

            default:
                break;
            }
        }
        mGridSearch.SetMapRange(std::max(GridWindSize.x, GridWindSize.y));
        if (data.find("PhysicsShapeS") != data.end())
        {
            for (size_t i = 0; i < data["PhysicsShapeS"].size(); ++i)
            {
                AddObject(new PhysicsShape(data["PhysicsShapeS"][i]));
            }
        }
        if (data.find("PhysicsParticleS") != data.end())
        {
            for (size_t i = 0; i < data["PhysicsParticleS"].size(); ++i)
            {
                AddObject(new PhysicsParticle(data["PhysicsParticleS"][i]));
            }
        }
        if (data.find("PhysicsCircleS") != data.end())
        {
            for (size_t i = 0; i < data["PhysicsCircleS"].size(); ++i)
            {
                AddObject(new PhysicsCircle(data["PhysicsCircleS"][i]));
            }
        }
        if (data.find("PhysicsLineS") != data.end())
        {
            for (size_t i = 0; i < data["PhysicsLineS"].size(); ++i)
            {
                AddObject(new PhysicsLine(data["PhysicsLineS"][i]));
            }
        }
        if (data.find("PhysicsJointS") != data.end())
        {
            for (size_t i = 0; i < data["PhysicsJointS"].size(); ++i)
            {
                PhysicsJoint *joint = new PhysicsJoint(data["PhysicsJointS"][i]);
                joint->body1 = (PhysicsAngle *)GetIndexPtr(data["PhysicsJointS"][i]["body1Type"], data["PhysicsJointS"][i]["body1"]);
                joint->body2 = (PhysicsAngle *)GetIndexPtr(data["PhysicsJointS"][i]["body2Type"], data["PhysicsJointS"][i]["body2"]);
                AddObject(joint);
            }
        }
        if (data.find("BaseJunctionS") != data.end())
        {
            PhysicsJunctionSS *JunctionSS;
            PhysicsJunctionS *JunctionS;
            PhysicsJunctionP *JunctionP;
            PhysicsJunctionPP *JunctionPP;
            for (size_t i = 0; i < data["BaseJunctionS"].size(); ++i)
            {
                switch ((CordObjectType)data["BaseJunctionS"][i]["ObjectType"])
                {
                case CordObjectType::JunctionAA:
                    JunctionSS = new PhysicsJunctionSS(data["BaseJunctionS"][i]);
                    JunctionSS->mParticle1 = (PhysicsAngle *)GetIndexPtr(data["BaseJunctionS"][i]["body1Type"], data["BaseJunctionS"][i]["body1"]);
                    JunctionSS->mParticle2 = (PhysicsAngle *)GetIndexPtr(data["BaseJunctionS"][i]["body2Type"], data["BaseJunctionS"][i]["body2"]);
                    AddObject(JunctionSS);
                    break;
                case CordObjectType::JunctionA:
                    JunctionS = new PhysicsJunctionS(data["BaseJunctionS"][i]);
                    JunctionS->mParticle = (PhysicsAngle *)GetIndexPtr(data["BaseJunctionS"][i]["body1Type"], data["BaseJunctionS"][i]["body1"]);
                    AddObject(JunctionS);
                    break;
                case CordObjectType::JunctionP:
                    JunctionP = new PhysicsJunctionP(data["BaseJunctionS"][i]);
                    JunctionP->mParticle = (PhysicsParticle *)GetIndexPtr(data["BaseJunctionS"][i]["body1Type"], data["BaseJunctionS"][i]["body1"]);
                    AddObject(JunctionP);
                    break;
                case CordObjectType::JunctionPP:
                    JunctionPP = new PhysicsJunctionPP(data["BaseJunctionS"][i]);
                    JunctionPP->mParticle1 = (PhysicsParticle *)GetIndexPtr(data["BaseJunctionS"][i]["body1Type"], data["BaseJunctionS"][i]["body1"]);
                    JunctionPP->mParticle2 = (PhysicsParticle *)GetIndexPtr(data["BaseJunctionS"][i]["body2Type"], data["BaseJunctionS"][i]["body2"]);
                    AddObject(JunctionPP);
                    break;

                default:
                    break;
                }
            }
        }
        if (data.find("CollideGroupVector") != data.end())
        {
#define CollideGroupVectorContrarySerialization(Arbiter_, Type1, Type2)                                                  \
    Type1##1 = (Type1 *)GetIndexPtr(data["CollideGroupVector"][i]["body1Type"], data["CollideGroupVector"][i]["body1"]); \
    Type2##2 = (Type2 *)GetIndexPtr(data["CollideGroupVector"][i]["body2Type"], data["CollideGroupVector"][i]["body2"]); \
    Arbiter_##Ptr = Pool##Arbiter_.newElement(Type1##1, Type2##2);                                                       \
    Arbiter_##Ptr->JsonContrarySerialization(data["CollideGroupVector"][i]);                                             \
    NewCollideGroup.push_back(Arbiter_##Ptr);

#define CollideGroupVectorContrarySerializationMapFormwork(Arbiter_, Type)                                             \
    Type##1 = (Type *)GetIndexPtr(data["CollideGroupVector"][i]["body1Type"], data["CollideGroupVector"][i]["body1"]); \
    Arbiter_##Ptr = Pool##Arbiter_.newElement(Type##1, wMapFormwork);                                                  \
    Arbiter_##Ptr->JsonContrarySerialization(data["CollideGroupVector"][i]);                                           \
    NewCollideGroup.push_back(Arbiter_##Ptr);

            PhysicsShape *PhysicsShape1, *PhysicsShape2;
            PhysicsParticle *PhysicsParticle1, *PhysicsParticle2;
            PhysicsCircle *PhysicsCircle1, *PhysicsCircle2;
            PhysicsLine *PhysicsLine1, *PhysicsLine2;
            PhysicsArbiterSS *PhysicsArbiterSSPtr;
            PhysicsArbiterSP *PhysicsArbiterSPPtr;
            PhysicsArbiterS *PhysicsArbiterSPtr;
            PhysicsArbiterP *PhysicsArbiterPPtr;
            PhysicsArbiterCS *PhysicsArbiterCSPtr;
            PhysicsArbiterCP *PhysicsArbiterCPPtr;
            PhysicsArbiterC *PhysicsArbiterCPtr;
            PhysicsArbiterCC *PhysicsArbiterCCPtr;
            PhysicsArbiterLC *PhysicsArbiterLCPtr;
            PhysicsArbiterLS *PhysicsArbiterLSPtr;
            PhysicsArbiterLP *PhysicsArbiterLPPtr;
            PhysicsArbiterL *PhysicsArbiterLPtr;
            for (size_t i = 0; i < data["CollideGroupVector"].size(); ++i)
            {
                switch (((PhysicsArbiterType)data["CollideGroupVector"][i]["type"]))
                {
                case PhysicsArbiterType::ArbiterSS:
                    CollideGroupVectorContrarySerialization(PhysicsArbiterSS, PhysicsShape, PhysicsShape);
                    break;
                case PhysicsArbiterType::ArbiterSP:
                    CollideGroupVectorContrarySerialization(PhysicsArbiterSP, PhysicsShape, PhysicsParticle);
                    break;
                case PhysicsArbiterType::ArbiterS:
                    CollideGroupVectorContrarySerializationMapFormwork(PhysicsArbiterS, PhysicsShape);
                    break;
                case PhysicsArbiterType::ArbiterP:
                    CollideGroupVectorContrarySerializationMapFormwork(PhysicsArbiterP, PhysicsParticle);
                    break;
                case PhysicsArbiterType::ArbiterC:
                    CollideGroupVectorContrarySerializationMapFormwork(PhysicsArbiterC, PhysicsCircle);
                    break;
                case PhysicsArbiterType::ArbiterCS:
                    CollideGroupVectorContrarySerialization(PhysicsArbiterCS, PhysicsCircle, PhysicsShape);
                    break;
                case PhysicsArbiterType::ArbiterCP:
                    CollideGroupVectorContrarySerialization(PhysicsArbiterCP, PhysicsCircle, PhysicsParticle);
                    break;
                case PhysicsArbiterType::ArbiterCC:
                    CollideGroupVectorContrarySerialization(PhysicsArbiterCC, PhysicsCircle, PhysicsCircle);
                    break;
                case PhysicsArbiterType::ArbiterLC:
                    CollideGroupVectorContrarySerialization(PhysicsArbiterLC, PhysicsLine, PhysicsCircle);
                    break;
                case PhysicsArbiterType::ArbiterLS:
                    CollideGroupVectorContrarySerialization(PhysicsArbiterLS, PhysicsLine, PhysicsShape);
                    break;
                case PhysicsArbiterType::ArbiterLP:
                    CollideGroupVectorContrarySerialization(PhysicsArbiterLP, PhysicsLine, PhysicsParticle);
                    break;
                case PhysicsArbiterType::ArbiterL:
                    CollideGroupVectorContrarySerializationMapFormwork(PhysicsArbiterL, PhysicsLine);
                    break;

                default:
                    break;
                }
            }
        }
    }

    unsigned int PhysicsWorld::GetPtrIndex(PhysicsFormwork *ptr)
    {
#define PhysicsVectorIndex(Vector, Ptr)        \
    for (size_t i = 0; i < Vector.size(); ++i) \
    {                                          \
        if (Vector[i] == Ptr)                  \
        {                                      \
            return i;                          \
        }                                      \
    }

        switch (ptr->PFGetType())
        {
        case PhysicsObjectEnum::shape:
            PhysicsVectorIndex(PhysicsShapeS, ptr);
            break;
        case PhysicsObjectEnum::particle:
            PhysicsVectorIndex(PhysicsParticleS, ptr);
            break;
        case PhysicsObjectEnum::circle:
            PhysicsVectorIndex(PhysicsCircleS, ptr);
            break;
        case PhysicsObjectEnum::line:
            PhysicsVectorIndex(PhysicsLineS, ptr);
            break;

        default:
            break;
        }
        return 0;
    }
    void *PhysicsWorld::GetIndexPtr(PhysicsObjectEnum Enum, unsigned int index)
    {
        switch (Enum)
        {
        case PhysicsObjectEnum::shape:
            return PhysicsShapeS[index];
            break;
        case PhysicsObjectEnum::particle:
            return PhysicsParticleS[index];
            break;
        case PhysicsObjectEnum::circle:
            return PhysicsCircleS[index];
            break;
        case PhysicsObjectEnum::line:
            return PhysicsLineS[index];
            break;

        default:
            break;
        }
        return 0;
    }
#endif
}
