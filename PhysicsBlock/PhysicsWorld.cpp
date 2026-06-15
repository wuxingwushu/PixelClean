#include "PhysicsWorld.hpp"
#include "BaseCalculate.hpp"
#include "MapStatic.hpp"
#include "MapDynamic.hpp"
#include <algorithm>
#include <map>
#include <iostream>
#include <climits>
#include "PhysicsBaseArbiter.hpp"
#include "PhysicsBaseCollide.hpp"
#include "PhysicsGPU.hpp"
#include <chrono>

// ========== thread_local 静态成员定义 ==========
#if ThreadPoolBool
thread_local PhysicsBlock::CollideOutput *PhysicsBlock::PhysicsWorld::tlCollideOutput = nullptr;
#endif

#if ThreadPoolBool
#define Map_Delete(key)                                 \
    if (CollideGroupS.find(key) != CollideGroupS.end()) \
    {                                                   \
        if (tlCollideOutput)                            \
            tlCollideOutput->deleteGroup.push_back(key);\
        else                                            \
            DeleteCollideGroup.push_back(key);           \
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

    /**
     * @brief 物理世界构造函数
     * @param gravityAcceleration 重力加速度
     * @param wind 是否启用网格风
     * @details 初始化重力加速度、网格风开关，设置网格搜索的线程数，
     *          并预分配多线程任务向量。 */
    PhysicsWorld::PhysicsWorld(Vec2_ gravityAcceleration, const bool wind) : GravityAcceleration(gravityAcceleration), WindBool(wind)
    {
        mGridSearch.SetThreadCount(std::thread::hardware_concurrency());
#if ThreadPoolBool
        const unsigned int xThreadNumHW = std::thread::hardware_concurrency();
        const unsigned int xThreadNum = std::min(xThreadNumHW, 8u);
        xTn.reserve(xThreadNum);
#endif
    }

    /**
     * @brief 物理世界析构函数
     * @details 按顺序清理所有物理资源：等待多线程任务结束、销毁所有碰撞对、释放网格风、
     *          销毁地图对象、销毁所有组装体及其子对象、销毁所有物理物体（形状、粒子、圆、关节、连接体、线）。 */
    PhysicsWorld::~PhysicsWorld()
    {
#if ThreadPoolBool
        // 等待线程结束
        for (auto &tf : xTn)
        {
            tf.wait();
        }
#endif

        for (auto i : CollideGroupS)
        {
            DeleteArbiter(i.second.arbiter);
        }
        CollideGroupS.clear();
        CollideGroupVector.clear();

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
        // 物理组装体（必须在类型向量之前销毁，因为 Assembly 的 RemoveFromWorld
        // 会通过 RemoveObject 将子对象从类型向量中移除并删除）
        for (auto i : PhysicsAssemblyS)
        {
            delete i;
        }
        PhysicsAssemblyS.clear();
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
    }

    /**
     * @brief 处理碰撞对
     * @param Ba 碰撞对指针
     * @details 计算两个物体之间的碰撞检测结果。如果有碰撞接触点，则将碰撞对插入或更新到
     *          CollideGroupS 中；若无碰撞且之前存在碰撞对，则标记为待删除。
     *          处理完毕后立即释放临时的 Ba 对象（通过内存池回收或 delete）。 */
    inline void PhysicsWorld::HandleCollideGroup(BaseArbiter *Ba)
    {
        Ba->ComputeCollide();
        auto it = CollideGroupS.find(Ba->key);

        if (Ba->numContacts > 0)
        {
            if (it == CollideGroupS.end())
            {
#if ThreadPoolBool
                if (tlCollideOutput)
                    tlCollideOutput->newGroup.push_back(Ba);
                else
                    NewCollideGroup.push_back(Ba);
#else
                NewCollideGroup.push_back(Ba);
#endif
                return;
            }
            it->second.arbiter->Update(Ba->contacts, Ba->numContacts);
        }
        else if (it != CollideGroupS.end())
        {
#if ThreadPoolBool
            if (tlCollideOutput)
                tlCollideOutput->deleteGroup.push_back(Ba->key);
            else
                DeleteCollideGroup.push_back(Ba->key);
#else
            DeleteCollideGroup.push_back(Ba->key);
#endif
        }
        DeleteArbiter(Ba);
    }

    /**
     * @brief 解析碰撞对变更
     * @details 将 NewCollideGroup 和 DeleteCollideGroup 中缓存的变更操作实际应用到
     *          CollideGroupS 和 CollideGroupVector 中。删除操作采用 swap-and-pop
     *          策略以保持 O(1) 复杂度。同时同步更新碰撞回调管理器中的碰撞对注册。 */
    inline void PhysicsWorld::ResolveCollideGroup()
    {
#if ThreadPoolBool
        for (auto &out : mCollideOutputs)
        {
            for (const auto &D : out.deleteGroup)
        {
            auto it = CollideGroupS.find(D);
            if (it == CollideGroupS.end())
            {
                continue;
            }
            BaseArbiter *arb = it->second.arbiter;
            int idx = it->second.vectorIndex;
            CollideGroupS.erase(it);

            mCollision.RemoveCollisionPair(arb);
            DeleteArbiter(arb);
            if (idx != (int)CollideGroupVector.size() - 1)
            {
                CollideGroupVector[idx] = CollideGroupVector.back();
                CollideGroupS[CollideGroupVector[idx]->key].vectorIndex = idx;
            }
            CollideGroupVector.pop_back();
        }
        out.deleteGroup.clear();

        CollideGroupVector.reserve(CollideGroupVector.size() + out.newGroup.size());
        for (auto &J : out.newGroup)
        {
            auto result = CollideGroupS.emplace(J->key, CollideGroupEntry{J, 0});
            if (result.second)
            {
                CollideGroupVector.push_back(J);
                result.first->second.vectorIndex = (int)CollideGroupVector.size() - 1;
                mCollision.AddCollisionPair(J);
            }
            else
            {
                DeleteArbiter(J);
            }
        }
        out.newGroup.clear();
        }
#else
        if (DeleteCollideGroup.empty() && NewCollideGroup.empty())
        {
            return;
        }

        for (const auto &D : DeleteCollideGroup)
        {
            auto it = CollideGroupS.find(D);
            if (it == CollideGroupS.end())
            {
                continue;
            }
            BaseArbiter *arb = it->second.arbiter;
            int idx = it->second.vectorIndex;
            CollideGroupS.erase(it);

            mCollision.RemoveCollisionPair(arb);
            DeleteArbiter(arb);
            if (idx != (int)CollideGroupVector.size() - 1)
            {
                CollideGroupVector[idx] = CollideGroupVector.back();
                CollideGroupS[CollideGroupVector[idx]->key].vectorIndex = idx;
            }
            CollideGroupVector.pop_back();
        }
        DeleteCollideGroup.clear();

        CollideGroupVector.reserve(CollideGroupVector.size() + NewCollideGroup.size());
        for (auto &J : NewCollideGroup)
        {
            auto result = CollideGroupS.emplace(J->key, CollideGroupEntry{J, 0});
            if (result.second)
            {
                CollideGroupVector.push_back(J);
                result.first->second.vectorIndex = (int)CollideGroupVector.size() - 1;
                mCollision.AddCollisionPair(J);
            }
            else
            {
                DeleteArbiter(J);
            }
        }
        NewCollideGroup.clear();
#endif
    }

    /**
     * @brief 物理仿真主循环
     * @param time 时间步长
     * @details 执行一帧完整的物理模拟，按顺序分为以下阶段：
     *          1. 解析上一帧的碰撞对变更（ResolveCollideGroup）
     *          2. 碰撞检测（多线程）：遍历所有物体，使用网格搜索查找潜在碰撞对，
     *             调用对应的 Arbiter 函数进行精确碰撞检测
     *          3. 预处理（PreStep）：为每个碰撞对、关节、连接体计算预求解参数
     *          4. 冲量迭代求解（ApplyImpulse）：多次迭代求解约束，支持 GPU 加速
     *          5. 位置更新（PhysicsPos）：根据速度和时间步长更新所有物体的位置
     *          6. 后处理：运动学物体更新、碰撞回调分发、触发器处理、网格搜索树更新
     *          7. 启动下一帧的碰撞检测任务（异步）
     *          各阶段耗时分别记录到对应的性能统计变量中。 */
    void PhysicsWorld::PhysicsEmulator(FLOAT_ time)
    {
        auto tEmulatorStart = std::chrono::high_resolution_clock::now();

#if ThreadPoolBool
        const unsigned int xThreadNumHW = std::thread::hardware_concurrency();
        const unsigned int xThreadNum = std::min(xThreadNumHW, 8u);

        // 等待 判断物体间的碰撞 的 任务结束
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
#endif
        ResolveCollideGroup();

        auto tCollisionEnd = std::chrono::high_resolution_clock::now();
        mCollisionDetectionTimeMS = std::chrono::duration<float, std::milli>(tCollisionEnd - tEmulatorStart).count();

        const auto XT_Fun = [this](int T_Num, int Tx, CollideOutput *output)
        {
            tlCollideOutput = output;

            bool JZ;
            int SizeD;
            int SizeY;
            std::vector<PhysicsBlock::PhysicsFormwork*> SearchV;

            ThreadTaskAllot(SizeD, SizeY, PhysicsShapeS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                // 静止了，跳过碰撞遍历
                JZ = (PhysicsShapeS[SizeD]->StaticNum > 10);

                // 和地形的碰撞
                if (!JZ && (PhysicsShapeS[SizeD]->mass != FLOAT_MAX))
                    Arbiter(PhysicsShapeS[SizeD], wMapFormwork);

                mGridSearch.Get(PhysicsShapeS[SizeD]->pos, PhysicsShapeS[SizeD]->radius, SearchV);
                for (auto i : SearchV)
                {

                    if ((PhysicsShapeS[SizeD]->mass == FLOAT_MAX) && (i->PFGetMass() == FLOAT_MAX))
                    {
                        continue;
                    }
                    if (JZ && (((PhysicsParticle *)i)->StaticNum > 10))
                    {
                        continue;
                    }

                    switch (i->PFGetType())
                    {
                    case PhysicsObjectEnum::circle:
                        if (SameAssembly(PhysicsShapeS[SizeD], i)) break;
                        if (!CollideAABB(PhysicsShapeS[SizeD], ((PhysicsCircle *)i)))
                        {
                            ArbiterKey key = ArbiterKey(((PhysicsCircle *)i), PhysicsShapeS[SizeD]);
                            Map_Delete(key);
                            break;
                        }
                        Arbiter(((PhysicsCircle *)i), PhysicsShapeS[SizeD]);
                        break;
                    case PhysicsObjectEnum::particle:
                        if (SameAssembly(PhysicsShapeS[SizeD], i)) break;
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
                        if ((PhysicsShape *)PhysicsShapeS[SizeD] <= (PhysicsShape *)i &&
                            PhysicsShapeS[SizeD]->radius <= ((PhysicsShape *)i)->radius) break;
                        if (SameAssembly(PhysicsShapeS[SizeD], i)) break;
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
                if (!JZ && (PhysicsCircleS[SizeD]->mass != FLOAT_MAX))
                    Arbiter(PhysicsCircleS[SizeD], wMapFormwork);

                mGridSearch.Get(PhysicsCircleS[SizeD]->pos, PhysicsCircleS[SizeD]->radius, SearchV);
                for (auto i : SearchV)
                {

                    if ((PhysicsCircleS[SizeD]->mass == FLOAT_MAX) && (i->PFGetMass() == FLOAT_MAX))
                    {
                        continue;
                    }
                    if (JZ && (((PhysicsParticle *)i)->StaticNum > 10))
                    {
                        continue;
                    }

                    switch (i->PFGetType())
                    {
                    case PhysicsObjectEnum::circle:
                        if ((PhysicsCircle *)PhysicsCircleS[SizeD] <= (PhysicsCircle *)i &&
                        PhysicsCircleS[SizeD]->radius <= ((PhysicsCircle *)i)->radius) break;
                        if (SameAssembly(PhysicsCircleS[SizeD], i)) break;
                        if (!CollideAABB(PhysicsCircleS[SizeD], ((PhysicsCircle *)i)))
                        {
                            ArbiterKey key = ArbiterKey(((PhysicsCircle *)i), PhysicsCircleS[SizeD]);
                            Map_Delete(key);
                            break;
                        }
                        Arbiter(((PhysicsCircle *)i), PhysicsCircleS[SizeD]);
                        break;
                    case PhysicsObjectEnum::particle:
                        if (SameAssembly(PhysicsCircleS[SizeD], i)) break;
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
                if (!JZ && (PhysicsLineS[SizeD]->mass != FLOAT_MAX))
                    Arbiter(PhysicsLineS[SizeD], wMapFormwork);

                mGridSearch.Get(PhysicsLineS[SizeD]->pos, PhysicsLineS[SizeD]->radius, SearchV);
                for (auto i : SearchV)
                {
                    if ((PhysicsLineS[SizeD]->mass == FLOAT_MAX) && (i->PFGetMass() == FLOAT_MAX))
                    {
                        continue;
                    }
                    if (JZ && (((PhysicsParticle *)i)->StaticNum > 10))
                    {
                        continue;
                    }

                    switch (i->PFGetType())
                    {
                    case PhysicsObjectEnum::circle:
                        if (SameAssembly(PhysicsLineS[SizeD], i)) break;
                        if (!CollideAABB(PhysicsLineS[SizeD], ((PhysicsCircle *)i)))
                        {
                            ArbiterKey key = ArbiterKey(((PhysicsCircle *)i), PhysicsLineS[SizeD]);
                            Map_Delete(key);
                            break;
                        }
                        Arbiter(PhysicsLineS[SizeD], ((PhysicsCircle *)i));
                        break;
                    case PhysicsObjectEnum::particle:
                        if (SameAssembly(PhysicsLineS[SizeD], i)) break;
                        Arbiter(PhysicsLineS[SizeD], ((PhysicsParticle *)i));
                        break;
                    case PhysicsObjectEnum::shape:
                        if (SameAssembly(PhysicsLineS[SizeD], i)) break;
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

            tlCollideOutput = nullptr;
        };

        FLOAT_ inv_dt = 1.0 / time;

        auto tPreStepStart = std::chrono::high_resolution_clock::now();

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

        auto tPreStepEnd = std::chrono::high_resolution_clock::now();
        mPreStepTimeMS = std::chrono::duration<float, std::milli>(tPreStepEnd - tPreStepStart).count();


        auto tImpulseStart = std::chrono::high_resolution_clock::now();

        if (mGPU && mGPU->IsReady() && mUseGPUApplyImpulse) {
            mApplyImpulseCPUTimeMS = 0.0f;
            mGPU->ExecuteGPUApplyImpulse(inv_dt, ApplyImpulseSize);
        }
        else 
        {
            
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
            for (size_t di = 0; di < ApplyImpulseSize; ++di)
            {
                xTn.reserve(xThreadNum);
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
            
                for (size_t i = 0; i < ApplyImpulseSize; ++i)
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
                
        }

        auto tImpulseEnd = std::chrono::high_resolution_clock::now();
        mApplyImpulseCPUTimeMS = std::chrono::duration<float, std::milli>(tImpulseEnd - tImpulseStart).count();

        auto tPosStart = std::chrono::high_resolution_clock::now();

// 这个多线程不影响结果
#if ThreadPoolBool
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
        xTn.reserve(xThreadNum);
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
        mKinematic.UpdateKinematicMotion(time);

        auto tPosEnd = std::chrono::high_resolution_clock::now();
        mPositionUpdateTimeMS = std::chrono::duration<float, std::milli>(tPosEnd - tPosStart).count();

        auto tPostStart = std::chrono::high_resolution_clock::now();

        mCollision.ProcessCollisions();

        {
            mTrigger.ProcessTriggers(mGridSearch);
        }

        // 更新搜索网格树
        if (mGridPositionDirty)
        {
            mGridSearch.UpdateGridOffset(mPendingGridCenter);
            mGridPositionDirty = false;
        }
        xTn.reserve(xThreadNum);
        for (unsigned int i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(&GridSearch::UpDaraWorkeTask, &mGridSearch, xThreadNum, i));
        }
        for (auto& tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
        mGridSearch.UpDaraWorkeTaskEnd();

#if ThreadPoolBool
        // 预分配每个线程的输出缓冲区
        mCollideOutputs.clear();
        mCollideOutputs.resize(xThreadNum);

        // 判断物体间的碰撞（不影响位置，所以可以不用强制等待完成）
        xTn.reserve(xThreadNum);
        for (unsigned int i = 0; i < xThreadNum; ++i)
        {
            mCollideOutputs[i].mThreadIndex = static_cast<unsigned char>(i);
            xTn.push_back(mThreadPool.enqueue(XT_Fun, i, xThreadNum, &mCollideOutputs[i]));
        }
#else
        // 碰撞检测
        XT_Fun(1, 1, nullptr);
#endif

        auto tPostEnd = std::chrono::high_resolution_clock::now();
        mPostProcessTimeMS = std::chrono::duration<float, std::milli>(tPostEnd - tPostStart).count();
    }

    /**
     * @brief 物理信息更新
     * @details 执行碰撞检测和预处理，更新网格搜索树。与 PhysicsEmulator 不同，
     *          此方法不执行冲量求解和位置更新，适用于反序列化恢复、静态场景初始化等
     *          无需完整物理模拟的场景。碰撞检测任务以异步方式启动。 */
    void PhysicsWorld::PhysicsInformationUpdate()
    {
#if ThreadPoolBool
        const unsigned int xThreadNumHW = std::thread::hardware_concurrency();
        const unsigned int xThreadNum = std::min(xThreadNumHW, 8u);

        // 等待 判断物体间的碰撞 任务结束
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
#endif
        ResolveCollideGroup();

                // 碰撞检测
        const auto XT_Fun = [this](int T_Num, int Tx, CollideOutput *output)
        {
            tlCollideOutput = output;

            int SizeD;
            int SizeY;
            std::vector<PhysicsBlock::PhysicsFormwork*> SearchV;
            ThreadTaskAllot(SizeD, SizeY, PhysicsShapeS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                Arbiter(PhysicsShapeS[SizeD], wMapFormwork);

                mGridSearch.Get(PhysicsShapeS[SizeD]->pos, PhysicsShapeS[SizeD]->radius, SearchV);
                for (auto i : SearchV)
                {

                    if ((PhysicsShapeS[SizeD]->mass == FLOAT_MAX) && (i->PFGetMass() == FLOAT_MAX))
                    {
                        continue;
                    }

                    switch (i->PFGetType())
                    {
                    case PhysicsObjectEnum::circle:
                        if (SameAssembly(PhysicsShapeS[SizeD], i)) break;
                        if (!CollideAABB(PhysicsShapeS[SizeD], ((PhysicsCircle *)i)))
                        {
                            ArbiterKey key = ArbiterKey(((PhysicsCircle *)i), PhysicsShapeS[SizeD]);
                            Map_Delete(key);
                            break;
                        }
                        Arbiter(((PhysicsCircle *)i), PhysicsShapeS[SizeD]);
                        break;
                    case PhysicsObjectEnum::particle:
                        if (SameAssembly(PhysicsShapeS[SizeD], i)) break;
                        if (PhysicsShapeS[SizeD]->DropCollision(((PhysicsParticle *)i)->pos).Collision)
                        {
                            ((PhysicsParticle *)i)->OldPosUpDataBool = false;
                            Arbiter(PhysicsShapeS[SizeD], ((PhysicsParticle *)i));
                        }
                        else
                        {
                            ArbiterKey key = ArbiterKey(PhysicsShapeS[SizeD], ((PhysicsParticle *)i));
                            Map_Delete(key);
                        }
                        break;
                    case PhysicsObjectEnum::shape:
                        if ((PhysicsShape *)PhysicsShapeS[SizeD] <= (PhysicsShape *)i &&
                            PhysicsShapeS[SizeD]->radius <= ((PhysicsShape *)i)->radius) break;
                        if (SameAssembly(PhysicsShapeS[SizeD], i)) break;
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

                mGridSearch.Get(PhysicsCircleS[SizeD]->pos, PhysicsCircleS[SizeD]->radius, SearchV);
                for (auto i : SearchV)
                {
                    if ((PhysicsCircleS[SizeD]->mass == FLOAT_MAX) && (i->PFGetMass() == FLOAT_MAX))
                    {
                        continue;
                    }

                    switch (i->PFGetType())
                    {
                    case PhysicsObjectEnum::circle:
                        if ((PhysicsCircle *)PhysicsCircleS[SizeD] <= (PhysicsCircle *)i &&
                        PhysicsCircleS[SizeD]->radius <= ((PhysicsCircle *)i)->radius) break;
                        if (SameAssembly(PhysicsCircleS[SizeD], i)) break;
                        if (!CollideAABB(PhysicsCircleS[SizeD], ((PhysicsCircle *)i)))
                        {
                            ArbiterKey key = ArbiterKey(((PhysicsCircle *)i), PhysicsCircleS[SizeD]);
                            Map_Delete(key);
                            break;
                        }
                        Arbiter(((PhysicsCircle *)i), PhysicsCircleS[SizeD]);
                        break;
                    case PhysicsObjectEnum::particle:
                        if (SameAssembly(PhysicsCircleS[SizeD], i)) break;
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

                mGridSearch.Get(PhysicsLineS[SizeD]->pos, PhysicsLineS[SizeD]->radius, SearchV);
                for (auto i : SearchV)
                {
                    if ((PhysicsLineS[SizeD]->mass == FLOAT_MAX) && (i->PFGetMass() == FLOAT_MAX))
                    {
                        continue;
                    }

                    switch (i->PFGetType())
                    {
                    case PhysicsObjectEnum::circle:
                        if (SameAssembly(PhysicsLineS[SizeD], i)) break;
                        if (!CollideAABB(PhysicsLineS[SizeD], ((PhysicsCircle *)i)))
                        {
                            ArbiterKey key = ArbiterKey(((PhysicsCircle *)i), PhysicsLineS[SizeD]);
                            Map_Delete(key);
                            break;
                        }
                        Arbiter(PhysicsLineS[SizeD], ((PhysicsCircle *)i));
                        break;
                    case PhysicsObjectEnum::particle:
                        if (SameAssembly(PhysicsLineS[SizeD], i)) break;
                        Arbiter(PhysicsLineS[SizeD], ((PhysicsParticle *)i));
                        break;
                    case PhysicsObjectEnum::shape:
                        if (SameAssembly(PhysicsLineS[SizeD], i)) break;
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

            tlCollideOutput = nullptr;
        };

        

#if ThreadPoolBool
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
        xTn.reserve(xThreadNum);
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

        // 更新搜索网格树
        xTn.reserve(xThreadNum);
        for (unsigned int i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(&GridSearch::UpDaraWorkeTask, &mGridSearch, xThreadNum, i));
        }
        for (auto& tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
        mGridSearch.UpDaraWorkeTaskEnd();

#if ThreadPoolBool
        // 预分配每个线程的输出缓冲区
        mCollideOutputs.clear();
        mCollideOutputs.resize(xThreadNum);

        // 判断物体间的碰撞（不影响位置，所以可以不用强制等待完成）
        xTn.reserve(xThreadNum);
        for (unsigned int i = 0; i < xThreadNum; ++i)
        {
            mCollideOutputs[i].mThreadIndex = static_cast<unsigned char>(i);
            xTn.push_back(mThreadPool.enqueue(XT_Fun, i, xThreadNum, &mCollideOutputs[i]));
        }
#else
        // 碰撞检测
        XT_Fun(1, 1, nullptr);
#endif
    }

    /**
     * @brief 等待碰撞检测线程完成
     * @details 阻塞等待所有正在运行的异步碰撞检测任务结束，通常在需要确保
     *          碰撞检测结果已经就绪时调用。 */
    void PhysicsWorld::WaitForCollisionThreads()
    {
#if ThreadPoolBool
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
#endif
    }

    /**
     * @brief 设置地图对象
     * @param MapFormwork_ 地图对象指针
     * @details 设置世界的地形碰撞对象，根据地图尺寸初始化网格风大小和网格搜索范围。
     *          如果启用了网格风，会分配对应大小的网格风数组并初始化为零。 */
    void PhysicsWorld::SetMapFormwork(MapFormwork *MapFormwork_)
    {
        if (MapFormwork_ == nullptr)
        {
            // 清除旧地图的通知桥接器
            if (wMapFormwork)
            {
                wMapFormwork->ClearCollisionChangeNotifier();
            }
            wMapFormwork = nullptr;
            return;
        }

        // 清除旧地图的通知桥接器（如有）
        if (wMapFormwork)
        {
            wMapFormwork->ClearCollisionChangeNotifier();
        }

        wMapFormwork = MapFormwork_;

        // 注入碰撞状态变化通知桥接器：地图 → PhysicsCollision
        MapFormwork_->SetCollisionChangeNotifier(
            [this, MapFormwork_](glm::ivec2 pos, bool state)
            {
                mCollision.OnMapCollisionChanged(MapFormwork_, pos, state);
            }
        );

        GridWindSize = MapFormwork_->FMGetMapSize();
        mGridSearch.SetMapRange(std::max(GridWindSize.x, GridWindSize.y));
        mGridCenter = mGridSearch.GetGridCenter();

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

    /**
     * @brief 根据焦点位置更新网格位置
     * @param focusPoint 焦点位置（如摄像机中心），世界坐标
     * @details 仅当焦点与上次重建中心距离超过阈值时才标记网格位置为脏，
     *          实际重建操作延迟到 PhysicsEmulator 中执行，避免重复计算。 */
    void PhysicsWorld::UpdateGridPosition(Vec2_ focusPoint)
    {
        if (glm::distance(focusPoint, mGridCenter) > mGridRebuildThreshold)
        {
            mPendingGridCenter = focusPoint;
            mGridCenter = focusPoint;
            mGridPositionDirty = true;
        }
    }

    /**
     * @brief 从物理世界中移除物体
     * @param Object 待移除的物理物体指针
     * @details 根据物体类型（形状、粒子、圆形、线段）从对应的类型列表中移除，
     *          同时清理所有与该物体关联的资源：关节、连接体（绳子/弹簧）、
     *          碰撞对（CollideGroupS 中的已激活碰撞对、NewCollideGroup 中待添加的碰撞对、
     *          DeleteCollideGroup 中待删除的碰撞对）。如果物体属于某个组装体，
     *          会先从组装体中移除子对象关联。最后销毁物体并减少对象计数。 */
    void PhysicsWorld::RemoveObject(PhysicsFormwork *Object)
    {
        if (Object->assembly)
        {
            Object->assembly->RemoveChild(Object);
        }
        switch (Object->PFGetType())
        {
        case PhysicsObjectEnum::shape:
        {
            PhysicsShape *shape = (PhysicsShape *)Object;
            mGridSearch.Remove(shape);
            for (size_t i = 0; i < PhysicsShapeS.size(); ++i)
            {
                if (PhysicsShapeS[i] == shape)
                {
                    PhysicsShapeS[i] = PhysicsShapeS.back();
                    PhysicsShapeS.pop_back();
                    break;
                }
            }
            for (int i = (int)PhysicsJointS.size() - 1; i >= 0; --i)
            {
                if (PhysicsJointS[i]->body1 == shape || PhysicsJointS[i]->body2 == shape)
                {
                    delete PhysicsJointS[i];
                    PhysicsJointS[i] = PhysicsJointS.back();
                    PhysicsJointS.pop_back();
                }
            }
            for (int i = (int)BaseJunctionS.size() - 1; i >= 0; --i)
            {
                bool remove = false;
                switch (BaseJunctionS[i]->ObjectType())
                {
                case CordObjectType::JunctionAA:
                    remove = (((PhysicsJunctionSS *)BaseJunctionS[i])->mParticle1 == shape ||
                              ((PhysicsJunctionSS *)BaseJunctionS[i])->mParticle2 == shape);
                    break;
                case CordObjectType::JunctionA:
                    remove = (((PhysicsJunctionS *)BaseJunctionS[i])->mParticle == shape);
                    break;
                default:
                    break;
                }
                if (remove)
                {
                    delete BaseJunctionS[i];
                    BaseJunctionS[i] = BaseJunctionS.back();
                    BaseJunctionS.pop_back();
                }
            }
            for (auto it = CollideGroupS.begin(); it != CollideGroupS.end(); )
            {
                if (it->first.object1 == shape || it->first.object2 == shape)
                {
                    BaseArbiter *arb = it->second.arbiter;
                    int idx = it->second.vectorIndex;
                    it = CollideGroupS.erase(it);

                    DeleteArbiter(arb);
                    if (idx != (int)CollideGroupVector.size() - 1)
                    {
                        CollideGroupVector[idx] = CollideGroupVector.back();
                        CollideGroupS[CollideGroupVector[idx]->key].vectorIndex = idx;
                    }
                    CollideGroupVector.pop_back();
                }
                else
                {
                    ++it;
                }
            }
            for (size_t i = 0; i < NewCollideGroup.size(); )
            {
                if (NewCollideGroup[i]->key.object1 == shape || NewCollideGroup[i]->key.object2 == shape)
                {
                    DeleteArbiter(NewCollideGroup[i]);
                    NewCollideGroup[i] = NewCollideGroup.back();
                    NewCollideGroup.pop_back();
                }
                else
                {
                    ++i;
                }
            }
            for (size_t i = 0; i < DeleteCollideGroup.size(); )
            {
                if (DeleteCollideGroup[i].object1 == shape || DeleteCollideGroup[i].object2 == shape)
                {
                    DeleteCollideGroup[i] = DeleteCollideGroup.back();
                    DeleteCollideGroup.pop_back();
                }
                else
                {
                    ++i;
                }
            }
            delete shape;
            break;
        }
        case PhysicsObjectEnum::particle:
        {
            PhysicsParticle *particle = (PhysicsParticle *)Object;
            mGridSearch.Remove(particle);
            for (size_t i = 0; i < PhysicsParticleS.size(); ++i)
            {
                if (PhysicsParticleS[i] == particle)
                {
                    PhysicsParticleS[i] = PhysicsParticleS.back();
                    PhysicsParticleS.pop_back();
                    break;
                }
            }
            for (int i = (int)BaseJunctionS.size() - 1; i >= 0; --i)
            {
                bool remove = false;
                switch (BaseJunctionS[i]->ObjectType())
                {
                case CordObjectType::JunctionP:
                    remove = (((PhysicsJunctionP *)BaseJunctionS[i])->mParticle == particle);
                    break;
                case CordObjectType::JunctionPP:
                    remove = (((PhysicsJunctionPP *)BaseJunctionS[i])->mParticle1 == particle ||
                              ((PhysicsJunctionPP *)BaseJunctionS[i])->mParticle2 == particle);
                    break;
                default:
                    break;
                }
                if (remove)
                {
                    delete BaseJunctionS[i];
                    BaseJunctionS[i] = BaseJunctionS.back();
                    BaseJunctionS.pop_back();
                }
            }
            for (auto it = CollideGroupS.begin(); it != CollideGroupS.end(); )
            {
                if (it->first.object1 == particle || it->first.object2 == particle)
                {
                    BaseArbiter *arb = it->second.arbiter;
                    int idx = it->second.vectorIndex;
                    it = CollideGroupS.erase(it);

                    DeleteArbiter(arb);
                    if (idx != (int)CollideGroupVector.size() - 1)
                    {
                        CollideGroupVector[idx] = CollideGroupVector.back();
                        CollideGroupS[CollideGroupVector[idx]->key].vectorIndex = idx;
                    }
                    CollideGroupVector.pop_back();
                }
                else
                {
                    ++it;
                }
            }
            for (size_t i = 0; i < NewCollideGroup.size(); )
            {
                if (NewCollideGroup[i]->key.object1 == particle || NewCollideGroup[i]->key.object2 == particle)
                {
                    DeleteArbiter(NewCollideGroup[i]);
                    NewCollideGroup[i] = NewCollideGroup.back();
                    NewCollideGroup.pop_back();
                }
                else
                {
                    ++i;
                }
            }
            for (size_t i = 0; i < DeleteCollideGroup.size(); )
            {
                if (DeleteCollideGroup[i].object1 == particle || DeleteCollideGroup[i].object2 == particle)
                {
                    DeleteCollideGroup[i] = DeleteCollideGroup.back();
                    DeleteCollideGroup.pop_back();
                }
                else
                {
                    ++i;
                }
            }
            // 自动清理该粒子的地形碰撞回调绑定
            mCollision.RemoveTerrainHitListener(particle);
            delete particle;
            break;
        }
        case PhysicsObjectEnum::circle:
        {
            PhysicsCircle *circle = (PhysicsCircle *)Object;
            mGridSearch.Remove(circle);
            for (size_t i = 0; i < PhysicsCircleS.size(); ++i)
            {
                if (PhysicsCircleS[i] == circle)
                {
                    PhysicsCircleS[i] = PhysicsCircleS.back();
                    PhysicsCircleS.pop_back();
                    break;
                }
            }
            for (int i = (int)PhysicsJointS.size() - 1; i >= 0; --i)
            {
                if (PhysicsJointS[i]->body1 == circle || PhysicsJointS[i]->body2 == circle)
                {
                    delete PhysicsJointS[i];
                    PhysicsJointS[i] = PhysicsJointS.back();
                    PhysicsJointS.pop_back();
                }
            }
            for (int i = (int)BaseJunctionS.size() - 1; i >= 0; --i)
            {
                bool remove = false;
                switch (BaseJunctionS[i]->ObjectType())
                {
                case CordObjectType::JunctionAA:
                    remove = (((PhysicsJunctionSS *)BaseJunctionS[i])->mParticle1 == circle ||
                              ((PhysicsJunctionSS *)BaseJunctionS[i])->mParticle2 == circle);
                    break;
                case CordObjectType::JunctionA:
                    remove = (((PhysicsJunctionS *)BaseJunctionS[i])->mParticle == circle);
                    break;
                default:
                    break;
                }
                if (remove)
                {
                    delete BaseJunctionS[i];
                    BaseJunctionS[i] = BaseJunctionS.back();
                    BaseJunctionS.pop_back();
                }
            }
            for (auto it = CollideGroupS.begin(); it != CollideGroupS.end(); )
            {
                if (it->first.object1 == circle || it->first.object2 == circle)
                {
                    BaseArbiter *arb = it->second.arbiter;
                    int idx = it->second.vectorIndex;
                    it = CollideGroupS.erase(it);

                    DeleteArbiter(arb);
                    if (idx != (int)CollideGroupVector.size() - 1)
                    {
                        CollideGroupVector[idx] = CollideGroupVector.back();
                        CollideGroupS[CollideGroupVector[idx]->key].vectorIndex = idx;
                    }
                    CollideGroupVector.pop_back();
                }
                else
                {
                    ++it;
                }
            }
            for (size_t i = 0; i < NewCollideGroup.size(); )
            {
                if (NewCollideGroup[i]->key.object1 == circle || NewCollideGroup[i]->key.object2 == circle)
                {
                    DeleteArbiter(NewCollideGroup[i]);
                    NewCollideGroup[i] = NewCollideGroup.back();
                    NewCollideGroup.pop_back();
                }
                else
                {
                    ++i;
                }
            }
            for (size_t i = 0; i < DeleteCollideGroup.size(); )
            {
                if (DeleteCollideGroup[i].object1 == circle || DeleteCollideGroup[i].object2 == circle)
                {
                    DeleteCollideGroup[i] = DeleteCollideGroup.back();
                    DeleteCollideGroup.pop_back();
                }
                else
                {
                    ++i;
                }
            }
            delete circle;
            break;
        }
        case PhysicsObjectEnum::line:
        {
            PhysicsLine *line = (PhysicsLine *)Object;
            mGridSearch.Remove(line);
            for (size_t i = 0; i < PhysicsLineS.size(); ++i)
            {
                if (PhysicsLineS[i] == line)
                {
                    PhysicsLineS[i] = PhysicsLineS.back();
                    PhysicsLineS.pop_back();
                    break;
                }
            }
            for (int i = (int)PhysicsJointS.size() - 1; i >= 0; --i)
            {
                if (PhysicsJointS[i]->body1 == line || PhysicsJointS[i]->body2 == line)
                {
                    delete PhysicsJointS[i];
                    PhysicsJointS[i] = PhysicsJointS.back();
                    PhysicsJointS.pop_back();
                }
            }
            for (auto it = CollideGroupS.begin(); it != CollideGroupS.end(); )
            {
                if (it->first.object1 == line || it->first.object2 == line)
                {
                    BaseArbiter *arb = it->second.arbiter;
                    int idx = it->second.vectorIndex;
                    it = CollideGroupS.erase(it);

                    DeleteArbiter(arb);
                    if (idx != (int)CollideGroupVector.size() - 1)
                    {
                        CollideGroupVector[idx] = CollideGroupVector.back();
                        CollideGroupS[CollideGroupVector[idx]->key].vectorIndex = idx;
                    }
                    CollideGroupVector.pop_back();
                }
                else
                {
                    ++it;
                }
            }
            for (size_t i = 0; i < NewCollideGroup.size(); )
            {
                if (NewCollideGroup[i]->key.object1 == line || NewCollideGroup[i]->key.object2 == line)
                {
                    DeleteArbiter(NewCollideGroup[i]);
                    NewCollideGroup[i] = NewCollideGroup.back();
                    NewCollideGroup.pop_back();
                }
                else
                {
                    ++i;
                }
            }
            for (size_t i = 0; i < DeleteCollideGroup.size(); )
            {
                if (DeleteCollideGroup[i].object1 == line || DeleteCollideGroup[i].object2 == line)
                {
                    DeleteCollideGroup[i] = DeleteCollideGroup.back();
                    DeleteCollideGroup.pop_back();
                }
                else
                {
                    ++i;
                }
            }
            delete line;
            break;
        }
        default:
            break;
        }
        if (ObjectSize > 0)
            --ObjectSize;
    }

    /**
     * @brief 从物理世界中移除组装体
     * @param Object 待移除的组装体指针
     * @details 调用组装体的 RemoveFromWorld 将其所有子对象从物理世界中移除，
     *          然后从组装体列表中移除该组装体并销毁。 */
    void PhysicsWorld::RemoveObject(PhysicsAssembly *Object)
    {
        Object->RemoveFromWorld(this);
        for (size_t i = 0; i < PhysicsAssemblyS.size(); ++i)
        {
            if (PhysicsAssemblyS[i] == Object)
            {
                PhysicsAssemblyS[i] = PhysicsAssemblyS.back();
                PhysicsAssemblyS.pop_back();
                break;
            }
        }
        delete Object;
    }

    /**
     * @brief 获取指定位置上的物理对象
     * @param pos 查询位置
     * @return 物理物体指针，若该位置无物体则返回 nullptr
     * @details 使用网格搜索查找 pos 附近的物体，然后进行精确命中判断：
     *          圆形：点与圆心距离小于半径；
     *          粒子：点与粒子位置距离小于 0.25；
     *          形状：点在碰撞范围内且投影碰撞检测通过；
     *          线段：点到线段的最近距离小于 0.25。 */
    PhysicsFormwork *PhysicsWorld::Get(Vec2_ pos)
    {
        std::vector<PhysicsBlock::PhysicsFormwork*> SearchV;
        mGridSearch.Get(pos, 1, SearchV);
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

    /**
     * @brief 获取世界内的总动能
     * @return 世界总动能
     * @details 遍历所有形状、粒子、圆形物体的动能（平动动能 + 转动动能）之和，
     *          计算公式为 E = Σ(½mv² + ½Iω²)。 */
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
        if (PhysicsAssemblyS.size() != 0)
        {
            dataIndex = 0;
            nlohmann::json_abi_v3_12_0::basic_json<> &dataArray = data["PhysicsAssemblyS"];
            dataArray = dataArray.array();
            for (auto ass : PhysicsAssemblyS)
            {
                ass->BuildChildDescriptors([&](PhysicsFormwork *child) {
                    return GetPtrIndex(child);
                });
                ass->JsonSerialization(dataArray[dataIndex]);
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
        mGridCenter = mGridSearch.GetGridCenter();
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
        if (data.find("PhysicsAssemblyS") != data.end())
        {
            for (size_t i = 0; i < data["PhysicsAssemblyS"].size(); ++i)
            {
                PhysicsAssembly *ass = new PhysicsAssembly(data["PhysicsAssemblyS"][i]);
                ass->ResolveChildren([&](PhysicsObjectEnum type, unsigned int idx) -> PhysicsFormwork * {
                    return (PhysicsFormwork *)GetIndexPtr(type, idx);
                });
                ass->SetWorld(this);
                PhysicsAssemblyS.push_back(ass);
            }
        }
        if (data.find("PhysicsJointS") != data.end())
        {
            for (size_t i = 0; i < data["PhysicsJointS"].size(); ++i)
            {
                PhysicsJoint *joint = new PhysicsJoint(data["PhysicsJointS"][i]);
                joint->body1 = (PhysicsAngle *)GetIndexPtr(data["PhysicsJointS"][i]["body1Type"], data["PhysicsJointS"][i]["body1"]);
                joint->body2 = (PhysicsAngle *)GetIndexPtr(data["PhysicsJointS"][i]["body2Type"], data["PhysicsJointS"][i]["body2"]);
                if (joint->body1 && joint->body2)
                    AddObject(joint);
                else
                    delete joint;
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
                    if (JunctionSS->mParticle1 && JunctionSS->mParticle2)
                        AddObject(JunctionSS);
                    else
                        delete JunctionSS;
                    break;
                case CordObjectType::JunctionA:
                    JunctionS = new PhysicsJunctionS(data["BaseJunctionS"][i]);
                    JunctionS->mParticle = (PhysicsAngle *)GetIndexPtr(data["BaseJunctionS"][i]["body1Type"], data["BaseJunctionS"][i]["body1"]);
                    if (JunctionS->mParticle)
                        AddObject(JunctionS);
                    else
                        delete JunctionS;
                    break;
                case CordObjectType::JunctionP:
                    JunctionP = new PhysicsJunctionP(data["BaseJunctionS"][i]);
                    JunctionP->mParticle = (PhysicsParticle *)GetIndexPtr(data["BaseJunctionS"][i]["body1Type"], data["BaseJunctionS"][i]["body1"]);
                    if (JunctionP->mParticle)
                        AddObject(JunctionP);
                    else
                        delete JunctionP;
                    break;
                case CordObjectType::JunctionPP:
                    JunctionPP = new PhysicsJunctionPP(data["BaseJunctionS"][i]);
                    JunctionPP->mParticle1 = (PhysicsParticle *)GetIndexPtr(data["BaseJunctionS"][i]["body1Type"], data["BaseJunctionS"][i]["body1"]);
                    JunctionPP->mParticle2 = (PhysicsParticle *)GetIndexPtr(data["BaseJunctionS"][i]["body2Type"], data["BaseJunctionS"][i]["body2"]);
                    if (JunctionPP->mParticle1 && JunctionPP->mParticle2)
                        AddObject(JunctionPP);
                    else
                        delete JunctionPP;
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
    if (Type1##1 && Type2##2)                                                                                            \
    {                                                                                                                    \
        Arbiter_##Ptr = Pool##Arbiter_[kMainThreadPoolIndex].newElement(Type1##1, Type2##2);                             \
        Arbiter_##Ptr->JsonContrarySerialization(data["CollideGroupVector"][i]);                                         \
        Arbiter_##Ptr->mAllocThreadIndex = static_cast<unsigned char>(kMainThreadPoolIndex);                             \
        NewCollideGroup.push_back(Arbiter_##Ptr);                                                                        \
    }

#define CollideGroupVectorContrarySerializationMapFormwork(Arbiter_, Type)                                             \
    Type##1 = (Type *)GetIndexPtr(data["CollideGroupVector"][i]["body1Type"], data["CollideGroupVector"][i]["body1"]); \
    if (Type##1 && wMapFormwork)                                                                                       \
    {                                                                                                                   \
        Arbiter_##Ptr = Pool##Arbiter_[kMainThreadPoolIndex].newElement(Type##1, wMapFormwork);                         \
        Arbiter_##Ptr->JsonContrarySerialization(data["CollideGroupVector"][i]);                                        \
        Arbiter_##Ptr->mAllocThreadIndex = static_cast<unsigned char>(kMainThreadPoolIndex);                            \
        NewCollideGroup.push_back(Arbiter_##Ptr);                                                                       \
    }

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
        return UINT_MAX;
    }
    void *PhysicsWorld::GetIndexPtr(PhysicsObjectEnum Enum, unsigned int index)
    {
        switch (Enum)
        {
        case PhysicsObjectEnum::shape:
            if (index < PhysicsShapeS.size())
                return PhysicsShapeS[index];
            break;
        case PhysicsObjectEnum::particle:
            if (index < PhysicsParticleS.size())
                return PhysicsParticleS[index];
            break;
        case PhysicsObjectEnum::circle:
            if (index < PhysicsCircleS.size())
                return PhysicsCircleS[index];
            break;
        case PhysicsObjectEnum::line:
            if (index < PhysicsLineS.size())
                return PhysicsLineS[index];
            break;

        default:
            break;
        }
        return nullptr;
    }
#endif
}
