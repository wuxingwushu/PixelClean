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
        for (auto& tf : xTn)
        {
            tf.wait();
        }

        if (GridWind != nullptr)
        {
            delete GridWind;
        }
        if (wMapFormwork != nullptr)
        {
            if (wMapFormwork->FMGetType() == _MapStatic) {
                delete (MapStatic *)wMapFormwork;
            }
            else if (wMapFormwork->FMGetType() == _MapDynamic) {
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
                return ((PhysicsLine*)i);
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
}
