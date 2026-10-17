#include "PhysicsWorld3D.hpp"

// 液体子系统：仅当总开关打开时才需要完整定义（PhysicsWorld3D.hpp 里只做了前向声明）。
// 打开 `PhysicsLiquid3DBool3D` 时必须包含本头文件，否则 mLiquid->Update() /
// ->NotifyParticleRemoved() 会因 incomplete type 编译失败。
#if PhysicsLiquid3DBool3D
#include "PhysicsLiquid3D.hpp"
#endif
#include "MapStatic3D.hpp"
#include "MapDynamic3D.hpp"
#include <cmath>
#include <algorithm>

namespace PhysicsBlock3D
{

#if ThreadPoolBool3D
    thread_local CollideOutput3D *PhysicsWorld3D::tlCollideOutput = nullptr;
#endif

    /**
     * @brief 本帧内用于标记"该碰撞对已消失"的删除宏
     * @details 与 2D 的 Map_Delete 同构：只对已登记的碰撞对入删除队列，
     *          未登记的对直接丢弃（否则删除阶段会空转查找）。
     *          多线程时写入本线程的 CollideOutput3D。
     */
#ifndef Map_Delete3D
#define Map_Delete3D(key)                                          \
    do                                                             \
    {                                                              \
        if (CollideGroupS.find(key) != CollideGroupS.end())        \
        {                                                          \
            if (tlCollideOutput)                                   \
                tlCollideOutput->deleteGroup.push_back(key);       \
            else                                                   \
                DeleteCollideGroup.push_back(key);                 \
        }                                                          \
    } while (0)
#endif

    // =====================================================================
    // 裁决器工厂与回收
    // =====================================================================
    //
    // 2D 顺序（PhysicsWorld.cpp:41-52）与 ID 完全照搬，3D 只换类型名：
    //   0 = CP(Circle,Particle)   1 = SP(Shape,Particle)
    //   2 = SS(Shape,Shape)       3 = S(Shape,MapFormwork)
    //   4 = P(Particle,Map)       5 = C(Circle,Map)
    //   6 = CS(Circle,Shape)      7 = CC(Circle,Circle)
    //   8 = LC(Line,Circle)       9 = LS(Line,Shape)
    //  10 = LP(Line,Particle)    11 = L(Line,Map)
    //
    // ⚠ 3D 的 Contact3D 明显大于 2D 的 Contact（Vec3_ 替代 Vec2_、多一个切向
    //   冲量分量与两个切向基向量），故每个内存池单块字节数按 1000*sizeof(Arbiter_)
    //   自动放大，无需手工调整。
    AuxiliaryArbiter3D(PhysicsArbiterCP3D, PhysicsCircle3D, C, PhysicsParticle3D, P, 0)
    AuxiliaryArbiter3D(PhysicsArbiterSP3D, PhysicsShape3D, S, PhysicsParticle3D, P, 1)
    AuxiliaryArbiter3D(PhysicsArbiterSS3D, PhysicsShape3D, S1, PhysicsShape3D, S2, 2)
    AuxiliaryArbiter3D(PhysicsArbiterS3D, PhysicsShape3D, S, MapFormwork3D, M, 3)
    AuxiliaryArbiter3D(PhysicsArbiterP3D, PhysicsParticle3D, P, MapFormwork3D, M, 4)
    AuxiliaryArbiter3D(PhysicsArbiterC3D, PhysicsCircle3D, C, MapFormwork3D, M, 5)
    AuxiliaryArbiter3D(PhysicsArbiterCS3D, PhysicsCircle3D, C, PhysicsShape3D, S, 6)
    AuxiliaryArbiter3D(PhysicsArbiterCC3D, PhysicsCircle3D, C1, PhysicsCircle3D, C2, 7)
    AuxiliaryArbiter3D(PhysicsArbiterLC3D, PhysicsLine3D, L, PhysicsCircle3D, C, 8)
    AuxiliaryArbiter3D(PhysicsArbiterLS3D, PhysicsLine3D, L, PhysicsShape3D, S, 9)
    AuxiliaryArbiter3D(PhysicsArbiterLP3D, PhysicsLine3D, L, PhysicsParticle3D, P, 10)
    AuxiliaryArbiter3D(PhysicsArbiterL3D, PhysicsLine3D, L, MapFormwork3D, M, 11)

    /**
     * @brief 按 key.PoolID 把裁决器归还到它所属线程的内存池
     * @param BA 待回收的裁决器
     * @details PoolID 在工厂里设置（0..11），mAllocThreadIndex 记录分配它的
     *          线程槽。3D 与 2D 完全同构，只是类型名换 3D。 */
    inline void PhysicsWorld3D::DeleteArbiter(BaseArbiter3D *BA)
    {
        switch (BA->key.PoolID)
        {
            AuxiliaryDelete3D(PhysicsArbiterCP3D, PhysicsCircle3D, C, PhysicsParticle3D, P, 0)
            AuxiliaryDelete3D(PhysicsArbiterSP3D, PhysicsShape3D, S, PhysicsParticle3D, P, 1)
            AuxiliaryDelete3D(PhysicsArbiterSS3D, PhysicsShape3D, S1, PhysicsShape3D, S2, 2)
            AuxiliaryDelete3D(PhysicsArbiterS3D, PhysicsShape3D, S, MapFormwork3D, M, 3)
            AuxiliaryDelete3D(PhysicsArbiterP3D, PhysicsParticle3D, P, MapFormwork3D, M, 4)
            AuxiliaryDelete3D(PhysicsArbiterC3D, PhysicsCircle3D, C, MapFormwork3D, M, 5)
            AuxiliaryDelete3D(PhysicsArbiterCS3D, PhysicsCircle3D, C, PhysicsShape3D, S, 6)
            AuxiliaryDelete3D(PhysicsArbiterCC3D, PhysicsCircle3D, C1, PhysicsCircle3D, C2, 7)
            AuxiliaryDelete3D(PhysicsArbiterLC3D, PhysicsLine3D, L, PhysicsCircle3D, C, 8)
            AuxiliaryDelete3D(PhysicsArbiterLS3D, PhysicsLine3D, L, PhysicsShape3D, S, 9)
            AuxiliaryDelete3D(PhysicsArbiterLP3D, PhysicsLine3D, L, PhysicsParticle3D, P, 10)
            AuxiliaryDelete3D(PhysicsArbiterL3D, PhysicsLine3D, L, MapFormwork3D, M, 11)
        default:
            break;
        }
    }

    // =====================================================================
    // 构造 / 析构
    // =====================================================================

    /**
     * @brief 构造物理世界
     * @param gravityAcceleration 重力加速度（3D）
     * @param wind 是否启用逐格风场
     * @details 与 2D 同构：初始化网格搜索的线程数并预留异步任务数组。 */
    PhysicsWorld3D::PhysicsWorld3D(Vec3_ gravityAcceleration, const bool wind)
        : GravityAcceleration(gravityAcceleration), WindBool(wind)
    {
        mGridSearch.SetThreadCount(std::thread::hardware_concurrency());
#if ThreadPoolBool3D
        const unsigned int xThreadNum = std::min(std::thread::hardware_concurrency(), 8u);
        xTn.reserve(xThreadNum);
#endif
    }

    /**
     * @brief 析构物理世界
     * @details 释放顺序至关重要（与 2D 一致）：
     *          ① 先等待上一帧异步碰撞任务结束（否则工作线程仍会触碰对象）；
     *          ② 回收所有活跃裁决器（它们持有物体裸指针）；
     *          ③ 释放液体系统（内部只持粒子指针）；
     *          ④ 释放逐格风场；
     *          ⑤ 释放地图；
     *          ⑥ **先释放组装体**——因为 ~PhysicsAssembly3D 会调
     *             RemoveFromWorld → RemoveObject，从而修改各类型向量；
     *             若先释放类型向量，组装体的子体指针就成了悬空。
     *          ⑦ 最后按类型释放剩余物体。
     *
     * @note 3D 相对 2D 的修正：`GridWind` 用 `delete[]` 释放
     *       （2D 是 `new Vec2_[...]` / `delete` 不匹配，属未定义行为）。 */
    PhysicsWorld3D::~PhysicsWorld3D()
    {
#if ThreadPoolBool3D
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
#endif
        // 回收所有裁决器
        for (auto &i : CollideGroupS)
        {
            DeleteArbiter(i.second.arbiter);
        }
        CollideGroupS.clear();
        CollideGroupVector.clear();

#if PhysicsLiquid3DBool3D
        delete mLiquid;
        mLiquid = nullptr;
#endif

        delete[] GridWind; // 修正 2D 的 new[]/delete 不匹配
        GridWind = nullptr;

        // 地图（按具体类型析构，因为 MapFormwork3D 无虚析构）
        if (wMapFormwork)
        {
            if (wMapFormwork->FMGetType() == _MapStatic)
            {
                delete (MapStatic3D *)wMapFormwork;
            }
            else if (wMapFormwork->FMGetType() == _MapDynamic)
            {
                delete (MapDynamic3D *)wMapFormwork;
            }
            wMapFormwork = nullptr;
        }

        // 组装体必须先于类型向量释放（其析构会回改类型向量）
        for (auto i : PhysicsAssemblyS)
        {
            delete i;
        }
        PhysicsAssemblyS.clear();

        for (auto i : PhysicsShapeS)
        {
            delete i;
        }
        PhysicsShapeS.clear();
        for (auto i : PhysicsParticleS)
        {
            delete i;
        }
        PhysicsParticleS.clear();
        for (auto i : PhysicsCircleS)
        {
            delete i;
        }
        PhysicsCircleS.clear();
        for (auto i : PhysicsJointS)
        {
            delete i;
        }
        PhysicsJointS.clear();
        for (auto i : BaseJunctionS)
        {
            delete i;
        }
        BaseJunctionS.clear();
        for (auto i : PhysicsLineS)
        {
            delete i;
        }
        PhysicsLineS.clear();
    }

    // =====================================================================
    // 碰撞对生命周期
    // =====================================================================

    /**
     * @brief 处理一个临时裁决器
     * @param Ba 临时裁决器（由内存池分配，本函数结束时归还）
     * @details 流程与 2D 完全一致：
     *          ① `ComputeCollide()` 计算接触点；
     *          ② 若 `numContacts > 0`：
     *             - 表中没有 → 放入"新增"队列并 **直接 return**（不回收，
     *               所有权移交 ResolveCollideGroup）；
     *             - 表中已有 → 用新接触点更新（热启动）已有裁决器；
     *          ③ 若 `numContacts == 0` 且表中已有 → 标记删除；
     *          ④ 回收临时裁决器。
     *          即 keep 规则 = `numContacts > 0`。 */
    inline void PhysicsWorld3D::HandleCollideGroup(BaseArbiter3D *Ba)
    {
        Ba->ComputeCollide();
        auto it = CollideGroupS.find(Ba->key);

        if (Ba->numContacts > 0)
        {
            if (it == CollideGroupS.end())
            {
#if ThreadPoolBool3D
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
#if ThreadPoolBool3D
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
     * @brief 把新增/删除队列实际应用到 CollideGroupS 与 CollideGroupVector
     * @details 删除用 swap-and-pop 保持 O(1)，并在搬移尾部元素后修正其
     *          vectorIndex；新增用 emplace 去重（重复则直接回收）。
     *          新增/删除都同步到 mCollision（触发 OnEnter/OnExit 回调）。
     *          多线程分支遍历各线程缓冲；单线程分支用全局队列。 */
    inline void PhysicsWorld3D::ResolveCollideGroup()
    {
#if ThreadPoolBool3D
        for (auto &out : mCollideOutputs)
        {
            for (const auto &D : out.deleteGroup)
            {
                auto it = CollideGroupS.find(D);
                if (it == CollideGroupS.end())
                {
                    continue;
                }
                BaseArbiter3D *arb = it->second.arbiter;
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
                auto result = CollideGroupS.emplace(J->key, CollideGroupEntry3D{J, 0});
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
            BaseArbiter3D *arb = it->second.arbiter;
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
            auto result = CollideGroupS.emplace(J->key, CollideGroupEntry3D{J, 0});
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
     * @brief 合并多线程碰撞输出
     * @details 预留接口：把各线程缓冲里的 newGroup/deleteGroup 汇总到全局队列。
     *          2D 只声明未定义（死声明），3D 补上实现以便外部调用。
     */
    void PhysicsWorld3D::MergeCollideOutputs(std::vector<CollideOutput3D> &outputs)
    {
        for (auto &out : outputs)
        {
            for (auto &J : out.newGroup)
            {
                NewCollideGroup.push_back(J);
            }
            for (const auto &D : out.deleteGroup)
            {
                DeleteCollideGroup.push_back(D);
            }
            out.clear();
        }
    }

    /**
     * @brief 物理仿真主循环（3D，九阶段）
     * @param time 时间步长
     * @details 与 2D PhysicsEmulator（PhysicsWorld.cpp:330-895）逐阶段对应：
     *          ① 等待上一帧异步入队的碰撞任务 → ResolveCollideGroup
     *          ② 构造 XT_Fun（多线程广相+窄相）
     *          ③ 外力/重力积分 PhysicsSpeed（先于约束求解）
     *          ④ PreStep（串行；2D 的多线程分支被 `& 0` 永久编译掉，
     *             原因：PreStep 含冲量副作用无法收敛，3D 保持同样决定）
     *          ⑤ ApplyImpulse 迭代
     *          ⑥ PhysicsPos 位置积分 + 运动学更新
     *          ⑦ 后处理：碰撞回调 → 触发器 → 网格平移/重建 → 液体 → 异步下一帧检测
     *
     * @note 3D 相对 2D 的差异：
     *       · 无 GPU 分支（3D 不含 PhysicsGPU 后端）；
     *       · `GridSearch` → `GridSearch3D`，`UpDaraWorkeTask` 签名一致；
     *       · `mTrigger.ProcessTriggers(mGridSearch)` 的粗筛在 3D 用立方体范围；
     *       · 液体段由 `PhysicsLiquid3DBool3D` 开关控制（当前为 0）。 */
    void PhysicsWorld3D::PhysicsEmulator(FLOAT_ time)
    {
        auto tEmulatorStart = std::chrono::high_resolution_clock::now();

#if ThreadPoolBool3D
        const unsigned int xThreadNumHW = std::thread::hardware_concurrency();
        const unsigned int xThreadNum = std::min(xThreadNumHW, 8u);

        // 等待上帧异步的碰撞检测结束
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
#endif
        ResolveCollideGroup();

        auto tCollisionEnd = std::chrono::high_resolution_clock::now();
        mCollisionDetectionTimeMS = std::chrono::duration<float, std::milli>(tCollisionEnd - tEmulatorStart).count();

        /**
         * @brief 碰撞检测工作函数（每线程一份）
         * @details 四个子扫描（Shape/Circle/Line/Particle），各自用
         *          ThreadTaskAllot3D 切分。粒子只做 (particle, map) 仲裁，
         *          不进网格宽相——与 2D 完全一致。
         *          3D 额外：所有物体都可能是液体粒子（IsLiquidParticle），
         *          固液交互由液体子系统自理，故一律跳过。 */
        const auto XT_Fun = [this](int T_Num, int Tx, CollideOutput3D *output)
        {
            tlCollideOutput = output;

            bool JZ;
            int SizeD;
            int SizeY;
            std::vector<PhysicsFormwork3D *> SearchV;

            // ---------- 形状 ----------
            ThreadTaskAllot3D(SizeD, SizeY, PhysicsShapeS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                JZ = (PhysicsShapeS[SizeD]->StaticNum > PhysicsSleepThreshold3D);

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
                    if (JZ && (static_cast<PhysicsParticle3D *>(i)->StaticNum > PhysicsSleepThreshold3D))
                    {
                        continue;
                    }

                    switch (i->PFGetType())
                    {
                    case PhysicsObjectEnum::circle:
                        if (SameAssembly(PhysicsShapeS[SizeD], i))
                            break;
                        if (!CollideAABB(PhysicsShapeS[SizeD], static_cast<PhysicsCircle3D *>(i)))
                        {
                            ArbiterKey3D key = ArbiterKey3D(static_cast<PhysicsCircle3D *>(i), PhysicsShapeS[SizeD]);
                            Map_Delete3D(key);
                            break;
                        }
                        Arbiter(static_cast<PhysicsCircle3D *>(i), PhysicsShapeS[SizeD]);
                        break;
                    case PhysicsObjectEnum::particle:
                        if (static_cast<PhysicsParticle3D *>(i)->IsLiquidParticle)
                            break; // 液体粒子：固液交互由液体系统自理
                        if (SameAssembly(PhysicsShapeS[SizeD], i))
                            break;
                        if (PhysicsShapeS[SizeD]->DropCollision(static_cast<PhysicsParticle3D *>(i)->pos).Collision)
                        {
                            static_cast<PhysicsParticle3D *>(i)->OldPosUpDataBool = false;
                            Arbiter(PhysicsShapeS[SizeD], static_cast<PhysicsParticle3D *>(i));
                        }
                        else
                        {
                            ArbiterKey3D key = ArbiterKey3D(PhysicsShapeS[SizeD], static_cast<PhysicsParticle3D *>(i));
                            Map_Delete3D(key);
                        }
                        break;
                    case PhysicsObjectEnum::shape:
                        // 防止同一对检测两次：只有"指针更小"或"半径更小"的一方才发起
                        if (static_cast<PhysicsShape3D *>(PhysicsShapeS[SizeD]) <= static_cast<PhysicsShape3D *>(i) &&
                            PhysicsShapeS[SizeD]->radius <= static_cast<PhysicsShape3D *>(i)->radius)
                            break;
                        if (SameAssembly(PhysicsShapeS[SizeD], i))
                            break;
                        if (!CollideAABB(PhysicsShapeS[SizeD], static_cast<PhysicsShape3D *>(i)))
                        {
                            ArbiterKey3D key = ArbiterKey3D(static_cast<PhysicsShape3D *>(i), PhysicsShapeS[SizeD]);
                            Map_Delete3D(key);
                            break;
                        }
                        Arbiter(static_cast<PhysicsShape3D *>(i), PhysicsShapeS[SizeD]);
                        break;

                    default:
                        break;
                    }
                }
            }

            // ---------- 圆球 ----------
            ThreadTaskAllot3D(SizeD, SizeY, PhysicsCircleS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                JZ = (PhysicsCircleS[SizeD]->StaticNum > PhysicsSleepThreshold3D);

                if (!JZ && (PhysicsCircleS[SizeD]->mass != FLOAT_MAX))
                    Arbiter(PhysicsCircleS[SizeD], wMapFormwork);

                mGridSearch.Get(PhysicsCircleS[SizeD]->pos, PhysicsCircleS[SizeD]->radius, SearchV);
                for (auto i : SearchV)
                {
                    if ((PhysicsCircleS[SizeD]->mass == FLOAT_MAX) && (i->PFGetMass() == FLOAT_MAX))
                    {
                        continue;
                    }
                    if (JZ && (static_cast<PhysicsParticle3D *>(i)->StaticNum > PhysicsSleepThreshold3D))
                    {
                        continue;
                    }

                    switch (i->PFGetType())
                    {
                    case PhysicsObjectEnum::circle:
                        if (static_cast<PhysicsCircle3D *>(PhysicsCircleS[SizeD]) <= static_cast<PhysicsCircle3D *>(i) &&
                            PhysicsCircleS[SizeD]->radius <= static_cast<PhysicsCircle3D *>(i)->radius)
                            break;
                        if (SameAssembly(PhysicsCircleS[SizeD], i))
                            break;
                        if (!CollideAABB(PhysicsCircleS[SizeD], static_cast<PhysicsCircle3D *>(i)))
                        {
                            ArbiterKey3D key = ArbiterKey3D(static_cast<PhysicsCircle3D *>(i), PhysicsCircleS[SizeD]);
                            Map_Delete3D(key);
                            break;
                        }
                        Arbiter(static_cast<PhysicsCircle3D *>(i), PhysicsCircleS[SizeD]);
                        break;
                    case PhysicsObjectEnum::particle:
                        if (static_cast<PhysicsParticle3D *>(i)->IsLiquidParticle)
                            break;
                        if (SameAssembly(PhysicsCircleS[SizeD], i))
                            break;
                        if (CollideAABB(PhysicsCircleS[SizeD], static_cast<PhysicsParticle3D *>(i)))
                        {
                            static_cast<PhysicsParticle3D *>(i)->OldPosUpDataBool = false;
                            Arbiter(PhysicsCircleS[SizeD], static_cast<PhysicsParticle3D *>(i));
                        }
                        else
                        {
                            ArbiterKey3D key = ArbiterKey3D(PhysicsCircleS[SizeD], static_cast<PhysicsParticle3D *>(i));
                            Map_Delete3D(key);
                        }
                        break;

                    default:
                        break;
                    }
                }
            }

            // ---------- 线段 ----------
            ThreadTaskAllot3D(SizeD, SizeY, PhysicsLineS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                JZ = (PhysicsLineS[SizeD]->StaticNum > PhysicsSleepThreshold3D);

                if (!JZ && (PhysicsLineS[SizeD]->mass != FLOAT_MAX))
                    Arbiter(PhysicsLineS[SizeD], wMapFormwork);

                mGridSearch.Get(PhysicsLineS[SizeD]->pos, PhysicsLineS[SizeD]->radius, SearchV);
                for (auto i : SearchV)
                {
                    if ((PhysicsLineS[SizeD]->mass == FLOAT_MAX) && (i->PFGetMass() == FLOAT_MAX))
                    {
                        continue;
                    }
                    if (JZ && (static_cast<PhysicsParticle3D *>(i)->StaticNum > PhysicsSleepThreshold3D))
                    {
                        continue;
                    }

                    switch (i->PFGetType())
                    {
                    case PhysicsObjectEnum::circle:
                        if (SameAssembly(PhysicsLineS[SizeD], i))
                            break;
                        if (!CollideAABB(PhysicsLineS[SizeD], static_cast<PhysicsCircle3D *>(i)))
                        {
                            ArbiterKey3D key = ArbiterKey3D(static_cast<PhysicsCircle3D *>(i), PhysicsLineS[SizeD]);
                            Map_Delete3D(key);
                            break;
                        }
                        Arbiter(PhysicsLineS[SizeD], static_cast<PhysicsCircle3D *>(i));
                        break;
                    case PhysicsObjectEnum::particle:
                        if (static_cast<PhysicsParticle3D *>(i)->IsLiquidParticle)
                            break;
                        if (SameAssembly(PhysicsLineS[SizeD], i))
                            break;
                        Arbiter(PhysicsLineS[SizeD], static_cast<PhysicsParticle3D *>(i));
                        break;
                    case PhysicsObjectEnum::shape:
                        if (SameAssembly(PhysicsLineS[SizeD], i))
                            break;
                        if (!CollideAABB(PhysicsLineS[SizeD], static_cast<PhysicsShape3D *>(i)))
                        {
                            ArbiterKey3D key = ArbiterKey3D(static_cast<PhysicsShape3D *>(i), PhysicsLineS[SizeD]);
                            Map_Delete3D(key);
                            break;
                        }
                        Arbiter(PhysicsLineS[SizeD], static_cast<PhysicsShape3D *>(i));
                        break;

                    default:
                        break;
                    }
                }
            }

            // ---------- 粒子（只和地形仲裁，不进网格宽相） ----------
            ThreadTaskAllot3D(SizeD, SizeY, PhysicsParticleS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                if (PhysicsParticleS[SizeD]->IsLiquidParticle ||
                    PhysicsParticleS[SizeD]->StaticNum > PhysicsSleepThreshold3D)
                {
                    continue;
                }

                Arbiter(PhysicsParticleS[SizeD], wMapFormwork);
            }

            tlCollideOutput = nullptr;
        };

        FLOAT_ inv_dt = 1.0 / time;

        // ===== 外力/重力积分（先于约束求解）=====
        // 顺序：v += dt*(g + F/m) → 冲量解算 → x += dt*v。
        // 外力当帧生效，接触解算器直接看到含重力的速度。
#if ThreadPoolBool3D
        const auto PhysicsSpeedXT_Fun = [this, time](int T_Num, int Tx)
        {
            int SizeD;
            int SizeY;
            ThreadTaskAllot3D(SizeD, SizeY, PhysicsShapeS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                PhysicsShapeS[SizeD]->PhysicsSpeed(time, GravityAcceleration);
            }
            ThreadTaskAllot3D(SizeD, SizeY, PhysicsParticleS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                if (PhysicsParticleS[SizeD]->IsLiquidParticle)
                {
                    continue;
                }
                PhysicsParticleS[SizeD]->PhysicsSpeed(time, GravityAcceleration);
            }
            ThreadTaskAllot3D(SizeD, SizeY, PhysicsCircleS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                PhysicsCircleS[SizeD]->PhysicsSpeed(time, GravityAcceleration);
            }
            ThreadTaskAllot3D(SizeD, SizeY, PhysicsLineS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                PhysicsLineS[SizeD]->PhysicsSpeed(time, GravityAcceleration);
            }
        };
        xTn.reserve(xThreadNum);
        for (size_t i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(PhysicsSpeedXT_Fun, i, xThreadNum));
        }
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
#else
        for (auto i : PhysicsShapeS)
        {
            i->PhysicsSpeed(time, GravityAcceleration);
        }
        for (auto i : PhysicsParticleS)
        {
            if (i->IsLiquidParticle)
            {
                continue;
            }
            i->PhysicsSpeed(time, GravityAcceleration);
        }
        for (auto i : PhysicsCircleS)
        {
            i->PhysicsSpeed(time, GravityAcceleration);
        }
        for (auto i : PhysicsLineS)
        {
            i->PhysicsSpeed(time, GravityAcceleration);
        }
#endif

        auto tPreStepStart = std::chrono::high_resolution_clock::now();

        // 预处理（串行）——PreStep 存在冲量副作用（速度/角速度），
        // 多线程无法收敛静止物体且增加不确定性，故与 2D 一样保持串行。
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

        auto tPreStepEnd = std::chrono::high_resolution_clock::now();
        mPreStepTimeMS = std::chrono::duration<float, std::milli>(tPreStepEnd - tPreStepStart).count();

        auto tImpulseStart = std::chrono::high_resolution_clock::now();

        {
            // 冲量迭代
#if (Definite != 1) & ThreadPoolBool3D
            const auto ApplyImpulseXT_Fun = [this](int T_Num, int Tx)
            {
                int SizeD;
                int SizeY;
                ThreadTaskAllot3D(SizeD, SizeY, CollideGroupVector.size(), T_Num, Tx);
                for (; SizeD < SizeY; ++SizeD)
                {
                    CollideGroupVector[SizeD]->ApplyImpulse();
                }
                ThreadTaskAllot3D(SizeD, SizeY, PhysicsJointS.size(), T_Num, Tx);
                for (; SizeD < SizeY; ++SizeD)
                {
                    PhysicsJointS[SizeD]->ApplyImpulse();
                }
                ThreadTaskAllot3D(SizeD, SizeY, BaseJunctionS.size(), T_Num, Tx);
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
                for (auto &tf : xTn)
                {
                    tf.wait();
                }
                xTn.clear();
            }
#else
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

#if ThreadPoolBool3D
        const auto PhysicsPosXT_Fun = [this, time](int T_Num, int Tx)
        {
            int SizeD;
            int SizeY;
            ThreadTaskAllot3D(SizeD, SizeY, PhysicsShapeS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                PhysicsShapeS[SizeD]->PhysicsPos(time, GravityAcceleration);
            }
            ThreadTaskAllot3D(SizeD, SizeY, PhysicsParticleS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                if (PhysicsParticleS[SizeD]->IsLiquidParticle)
                {
                    continue;
                }
                PhysicsParticleS[SizeD]->PhysicsPos(time, GravityAcceleration);
            }
            ThreadTaskAllot3D(SizeD, SizeY, PhysicsCircleS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                PhysicsCircleS[SizeD]->PhysicsPos(time, GravityAcceleration);
            }
            ThreadTaskAllot3D(SizeD, SizeY, PhysicsLineS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                PhysicsLineS[SizeD]->PhysicsPos(time, GravityAcceleration);
            }
        };
        xTn.reserve(xThreadNum);
        for (size_t i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(PhysicsPosXT_Fun, i, xThreadNum));
        }
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
#else
        for (auto i : PhysicsShapeS)
        {
            i->PhysicsPos(time, GravityAcceleration);
        }
        for (auto i : PhysicsParticleS)
        {
            if (i->IsLiquidParticle)
            {
                continue;
            }
            i->PhysicsPos(time, GravityAcceleration);
        }
        for (auto i : PhysicsCircleS)
        {
            i->PhysicsPos(time, GravityAcceleration);
        }
        for (auto i : PhysicsLineS)
        {
            i->PhysicsPos(time, GravityAcceleration);
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
#if ThreadPoolBool3D
        xTn.reserve(xThreadNum);
        for (unsigned int i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(&GridSearch3D::UpDaraWorkeTask, &mGridSearch, xThreadNum, i));
        }
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
#endif
        mGridSearch.UpDaraWorkeTaskEnd();

#if PhysicsLiquid3DBool3D
        if (mLiquid != nullptr)
        {
            mLiquid->Update(time);
        }
#endif

#if ThreadPoolBool3D
        // 预分配每个线程的输出缓冲区
        mCollideOutputs.clear();
        mCollideOutputs.resize(xThreadNum);

        // 启动下一帧的碰撞检测（不影响本帧位置，故意不等待）
        xTn.reserve(xThreadNum);
        for (unsigned int i = 0; i < xThreadNum; ++i)
        {
            mCollideOutputs[i].mThreadIndex = static_cast<unsigned char>(i);
            xTn.push_back(mThreadPool.enqueue(XT_Fun, i, xThreadNum, &mCollideOutputs[i]));
        }
#else
        XT_Fun(1, 1, nullptr);
#endif

        auto tPostEnd = std::chrono::high_resolution_clock::now();
        mPostProcessTimeMS = std::chrono::duration<float, std::milli>(tPostEnd - tPostStart).count();
    }

    /**
     * @brief 仅更新碰撞信息（不积分、不解算、不移动）
     * @details 与 2D PhysicsInformationUpdate（PhysicsWorld.cpp:902-1157）对应。
     *          用于反序列化恢复、静态场景初始化等：把 CollideGroupS 填好，
     *          但不推进时间。与 PhysicsEmulator 的差异：
     *          · 碰撞检测去掉 StaticNum 休眠跳过（始终检测）；
     *          · PreStep 走**无参**版本 `i->PreStep()`；
     *          · 只解算一次冲量迭代。 */
    void PhysicsWorld3D::PhysicsInformationUpdate()
    {
#if ThreadPoolBool3D
        const unsigned int xThreadNumHW = std::thread::hardware_concurrency();
        const unsigned int xThreadNum = std::min(xThreadNumHW, 8u);
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
#endif
        ResolveCollideGroup();

        const auto XT_Fun = [this](int T_Num, int Tx, CollideOutput3D *output)
        {
            tlCollideOutput = output;

            int SizeD;
            int SizeY;
            std::vector<PhysicsFormwork3D *> SearchV;

            ThreadTaskAllot3D(SizeD, SizeY, PhysicsShapeS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                if (PhysicsShapeS[SizeD]->mass != FLOAT_MAX)
                    Arbiter(PhysicsShapeS[SizeD], wMapFormwork);

                mGridSearch.Get(PhysicsShapeS[SizeD]->pos, PhysicsShapeS[SizeD]->radius, SearchV);
                for (auto i : SearchV)
                {
                    if ((PhysicsShapeS[SizeD]->mass == FLOAT_MAX) && (i->PFGetMass() == FLOAT_MAX))
                        continue;

                    switch (i->PFGetType())
                    {
                    case PhysicsObjectEnum::circle:
                        if (SameAssembly(PhysicsShapeS[SizeD], i))
                            break;
                        if (!CollideAABB(PhysicsShapeS[SizeD], static_cast<PhysicsCircle3D *>(i)))
                        {
                            Map_Delete3D(ArbiterKey3D(static_cast<PhysicsCircle3D *>(i), PhysicsShapeS[SizeD]));
                            break;
                        }
                        Arbiter(static_cast<PhysicsCircle3D *>(i), PhysicsShapeS[SizeD]);
                        break;
                    case PhysicsObjectEnum::particle:
                        if (static_cast<PhysicsParticle3D *>(i)->IsLiquidParticle)
                            break;
                        if (SameAssembly(PhysicsShapeS[SizeD], i))
                            break;
                        if (PhysicsShapeS[SizeD]->DropCollision(static_cast<PhysicsParticle3D *>(i)->pos).Collision)
                            Arbiter(PhysicsShapeS[SizeD], static_cast<PhysicsParticle3D *>(i));
                        else
                            Map_Delete3D(ArbiterKey3D(PhysicsShapeS[SizeD], static_cast<PhysicsParticle3D *>(i)));
                        break;
                    case PhysicsObjectEnum::shape:
                        if (static_cast<PhysicsShape3D *>(PhysicsShapeS[SizeD]) <= static_cast<PhysicsShape3D *>(i) &&
                            PhysicsShapeS[SizeD]->radius <= static_cast<PhysicsShape3D *>(i)->radius)
                            break;
                        if (SameAssembly(PhysicsShapeS[SizeD], i))
                            break;
                        if (!CollideAABB(PhysicsShapeS[SizeD], static_cast<PhysicsShape3D *>(i)))
                        {
                            Map_Delete3D(ArbiterKey3D(static_cast<PhysicsShape3D *>(i), PhysicsShapeS[SizeD]));
                            break;
                        }
                        Arbiter(static_cast<PhysicsShape3D *>(i), PhysicsShapeS[SizeD]);
                        break;

                    default:
                        break;
                    }
                }
            }

            ThreadTaskAllot3D(SizeD, SizeY, PhysicsParticleS.size(), T_Num, Tx);
            for (; SizeD < SizeY; ++SizeD)
            {
                if (PhysicsParticleS[SizeD]->IsLiquidParticle)
                    continue;
                Arbiter(PhysicsParticleS[SizeD], wMapFormwork);
            }

            tlCollideOutput = nullptr;
        };

#if ThreadPoolBool3D
        mCollideOutputs.clear();
        mCollideOutputs.resize(xThreadNum);
        for (unsigned int i = 0; i < xThreadNum; ++i)
        {
            mCollideOutputs[i].mThreadIndex = static_cast<unsigned char>(i);
            xTn.push_back(mThreadPool.enqueue(XT_Fun, i, xThreadNum, &mCollideOutputs[i]));
        }
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
#else
        XT_Fun(1, 1, nullptr);
#endif

        // 无参 PreStep + 迭代冲量
        for (auto i : CollideGroupVector)
        {
            i->PreStep();
        }
        // 与 2D（PhysicsWorld.cpp:742-756）一致：顺序冲量迭代 ApplyImpulseSize 次。
        // 只解一轮时堆叠中下层拿不到上层真实反作用力，塔会持续抖动/被弹开。
        // 迭代方向逐轮交替：顺序 Gauss-Seidel 里排在后面的接触总是最后被修正，
        // 固定顺序会在对称的支撑面上留下净力矩（方块塔会缓慢倾斜）。
        for (unsigned int it3D = 0; it3D < ApplyImpulseSize; ++it3D)
        {
            if ((it3D & 1u) == 0u)
            {
                for (auto kv : CollideGroupVector)
                {
                    kv->ApplyImpulse();
                }
            }
            else
            {
                for (auto kv = CollideGroupVector.rbegin(); kv != CollideGroupVector.rend(); ++kv)
                {
                    (*kv)->ApplyImpulse();
                }
            }
        }

        // 重建网格树
#if ThreadPoolBool3D
        xTn.reserve(xThreadNum);
        for (unsigned int i = 0; i < xThreadNum; ++i)
        {
            xTn.push_back(mThreadPool.enqueue(&GridSearch3D::UpDaraWorkeTask, &mGridSearch, xThreadNum, i));
        }
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
#endif
        mGridSearch.UpDaraWorkeTaskEnd();
    }

    /**
     * @brief 等待上一帧异步发起的碰撞检测任务结束
     * @details 与 2D 同构。析构与场景切换前应调用。 */
    void PhysicsWorld3D::WaitForCollisionThreads()
    {
#if ThreadPoolBool3D
        for (auto &tf : xTn)
        {
            tf.wait();
        }
        xTn.clear();
#endif
    }

    /**
     * @brief 绑定地图
     * @param MapFormwork_ 新地图（可为 nullptr 表示解绑）
     * @details 与 2D（PhysicsWorld.cpp:1179-1225）同构：
     *          ① 解绑时先清掉旧地图的碰撞变更通知器；
     *          ② 注入桥接 lambda，把地图的 (体素, 状态) 变更转发给
     *             `mCollision.OnMapCollisionChanged`；
     *          ③ 按地图尺寸重设空间网格范围（3D 取三轴最大值）；
     *          ④ 若启用风场，按新尺寸重建逐格风场数组。
     *
     * @note 3D 修正：`GridWind` 用 `delete[]` 释放（2D 是 `delete`，不匹配）。 */
    void PhysicsWorld3D::SetMapFormwork(MapFormwork3D *MapFormwork_)
    {
        if (MapFormwork_ == nullptr)
        {
            if (wMapFormwork)
            {
                wMapFormwork->ClearCollisionChangeNotifier();
            }
            wMapFormwork = nullptr;
            return;
        }

        if (wMapFormwork)
        {
            wMapFormwork->ClearCollisionChangeNotifier();
        }

        wMapFormwork = MapFormwork_;

        // 注入桥接器：地图内碰撞格发生变化时通知碰撞回调系统
        MapFormwork_->SetCollisionChangeNotifier(
            [this, MapFormwork_](IVec3_ pos, bool state)
            {
                mCollision.OnMapCollisionChanged(MapFormwork_, pos, state);
            });

        GridWindSize = MapFormwork_->FMGetMapSize();
        mGridSearch.SetMapRange(std::max(std::max(GridWindSize.x, GridWindSize.y), GridWindSize.z));
        mGridCenter = mGridSearch.GetGridCenter();

        if (!WindBool)
        {
            return;
        }

        delete[] GridWind; // 修正 2D 的 new[]/delete 不匹配
        GridWind = new Vec3_[size_t(GridWindSize.x) * GridWindSize.y * GridWindSize.z];
        for (size_t i = 0; i < size_t(GridWindSize.x) * GridWindSize.y * GridWindSize.z; ++i)
        {
            GridWind[i] = Vec3_(0.0f);
        }
    }

    /**
     * @brief 按焦点位置决定是否平移空间网格
     * @param focusPoint 关注点（通常是主摄像机/玩家位置）
     * @details 若焦点离网格中心超过阈值，则标记脏位，实际平移推迟到
     *          后处理阶段（避免在仿真中途改网格）。与 2D 同构。 */
    void PhysicsWorld3D::UpdateGridPosition(Vec3_ focusPoint)
    {
        if (glm::distance(focusPoint, mGridCenter) > mGridRebuildThreshold)
        {
            mPendingGridCenter = focusPoint;
            mGridCenter = focusPoint;
            mGridPositionDirty = true;
        }
    }

    // =====================================================================
    // 对象移除
    // =====================================================================

    /**
     * @brief 从物理世界中移除并销毁一个物理物体
     * @param Object 待移除的物体（形状 / 粒子 / 圆球 / 线段）
     * @details 与 2D 同构，按类型分四路，每路统一做五件事：
     *          ① 从空间网格摘除（线段不入网格，故跳过）；
     *          ② 从对应类型向量 swap-and-pop；
     *          ③ 销毁以它为端点的关节 PhysicsJoint3D；
     *          ④ 销毁以它为端点的连接 BaseJunction3D（形状/圆球查 AA 与 A，粒子查 P 与 PP）；
     *          ⑤ 清理碰撞对表 CollideGroupS / NewCollideGroup / DeleteCollideGroup；
     *          ⑥ `delete` 本体，并递减 ObjectSize。
     *          粒子额外：解绑地形碰撞回调、通知液体系统摘指针。
     *
     * @note 3D 相对 2D 的整理：把重复四遍的碰撞对清理抽成 lambda
     *       `eraseArbiterCleanup`，并用 `PhysicsSleepThreshold3D` 语义保持一致的删除路径。
     */
    void PhysicsWorld3D::RemoveObject(PhysicsFormwork3D *Object)
    {
        if (Object->assembly)
        {
            Object->assembly->RemoveChild(Object);
        }

        // ---- 局部辅助：从任意同构 vector 中 swap-and-pop 掉一个指针 ----
        auto erasePtr = [](auto &vec, auto *p)
        {
            for (size_t i = 0; i < vec.size(); ++i)
            {
                if (vec[i] == p)
                {
                    vec[i] = vec.back();
                    vec.pop_back();
                    return;
                }
            }
        };

        // ---- 局部辅助：销毁所有以 body 为端点的关节 ----
        auto eraseJoints = [this](PhysicsAngle3D *body)
        {
            for (int i = (int)PhysicsJointS.size() - 1; i >= 0; --i)
            {
                if (PhysicsJointS[i]->body1 == body || PhysicsJointS[i]->body2 == body)
                {
                    delete PhysicsJointS[i];
                    PhysicsJointS[i] = PhysicsJointS.back();
                    PhysicsJointS.pop_back();
                }
            }
        };

        // ---- 局部辅助：销毁所有以 body（形状/圆球/线段）为端点的连接 ----
        auto eraseJunctionsAngle = [this](PhysicsAngle3D *body)
        {
            for (int i = (int)BaseJunctionS.size() - 1; i >= 0; --i)
            {
                bool remove = false;
                switch (BaseJunctionS[i]->ObjectType())
                {
                case JunctionAA3D:
                    remove = (((PhysicsJunctionSS3D *)BaseJunctionS[i])->mParticle1 == body ||
                              ((PhysicsJunctionSS3D *)BaseJunctionS[i])->mParticle2 == body);
                    break;
                case JunctionA3D:
                    remove = (((PhysicsJunctionS3D *)BaseJunctionS[i])->mParticle == body);
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
        };

        // ---- 局部辅助：销毁所有以 particle 为端点的连接 ----
        auto eraseJunctionsParticle = [this](PhysicsParticle3D *particle)
        {
            for (int i = (int)BaseJunctionS.size() - 1; i >= 0; --i)
            {
                bool remove = false;
                switch (BaseJunctionS[i]->ObjectType())
                {
                case JunctionP3D:
                    remove = (((PhysicsJunctionP3D *)BaseJunctionS[i])->mParticle == particle);
                    break;
                case JunctionPP3D:
                    remove = (((PhysicsJunctionPP3D *)BaseJunctionS[i])->mParticle1 == particle ||
                              ((PhysicsJunctionPP3D *)BaseJunctionS[i])->mParticle2 == particle);
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
        };

        // ---- 局部辅助：清理碰撞对表（三个容器） ----
        auto eraseArbiterCleanup = [this](const void *objKey)
        {
            for (auto it = CollideGroupS.begin(); it != CollideGroupS.end();)
            {
                if (it->first.object1 == objKey || it->first.object2 == objKey)
                {
                    BaseArbiter3D *arb = it->second.arbiter;
                    const int idx = it->second.vectorIndex;
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
            for (size_t i = 0; i < NewCollideGroup.size();)
            {
                if (NewCollideGroup[i]->key.object1 == objKey || NewCollideGroup[i]->key.object2 == objKey)
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
            for (size_t i = 0; i < DeleteCollideGroup.size();)
            {
                if (DeleteCollideGroup[i].object1 == objKey || DeleteCollideGroup[i].object2 == objKey)
                {
                    DeleteCollideGroup[i] = DeleteCollideGroup.back();
                    DeleteCollideGroup.pop_back();
                }
                else
                {
                    ++i;
                }
            }
        };

        switch (Object->PFGetType())
        {
        case PhysicsObjectEnum::shape:
        {
            PhysicsShape3D *shape = (PhysicsShape3D *)Object;
            mGridSearch.Remove(shape);
            erasePtr(PhysicsShapeS, shape);
            eraseJoints(shape);
            eraseJunctionsAngle(shape);
            eraseArbiterCleanup(shape);
            delete shape;
            break;
        }
        case PhysicsObjectEnum::particle:
        {
            PhysicsParticle3D *particle = (PhysicsParticle3D *)Object;
            mGridSearch.Remove(particle);
            erasePtr(PhysicsParticleS, particle);
            eraseJunctionsParticle(particle);
            eraseArbiterCleanup(particle);
            // 自动清理该粒子的地形碰撞回调绑定
            mCollision.RemoveTerrainHitListener(particle);
#if PhysicsLiquid3DBool3D
            // 通知液体系统先摘掉指针，避免其内部列表悬空
            if (mLiquid != nullptr)
            {
                mLiquid->NotifyParticleRemoved(particle);
            }
#endif
            delete particle;
            break;
        }
        case PhysicsObjectEnum::circle:
        {
            PhysicsCircle3D *circle = (PhysicsCircle3D *)Object;
            mGridSearch.Remove(circle);
            erasePtr(PhysicsCircleS, circle);
            eraseJoints(circle);
            eraseJunctionsAngle(circle);
            eraseArbiterCleanup(circle);
            delete circle;
            break;
        }
        case PhysicsObjectEnum::line:
        {
            PhysicsLine3D *line = (PhysicsLine3D *)Object;
            // 2D 的 line 分支也调了 mGridSearch.Remove（虽然 AddObject 未入网格），
            // 3D 保留同样行为：Remove 对不在网格中的对象是安全的空操作。
            mGridSearch.Remove(line);
            erasePtr(PhysicsLineS, line);
            eraseJoints(line);
            eraseArbiterCleanup(line);
            delete line;
            break;
        }
        default:
            break;
        }

        if (ObjectSize > 0)
        {
            --ObjectSize;
        }
    }

    /**
     * @brief 从物理世界中移除并销毁一个组装体
     * @param Object 待移除的组装体
     * @details 先让组装体把所有子体从世界移除（会回调 RemoveObject），
     *          再把组装体自身从列表 swap-and-pop 并销毁。与 2D 同构。 */
    void PhysicsWorld3D::RemoveObject(PhysicsAssembly3D *Object)
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

    // =====================================================================
    // 查询
    // =====================================================================

    /**
     * @brief 获取指定位置上的物理对象
     * @param pos 查询位置（世界坐标）
     * @return 命中的物体指针，未命中返回 nullptr
     * @details 先用空间网格做球查询粗筛，再按类型精确判定：
     *          - 圆球：点到球心距离 < 半径；
     *          - 粒子：点到粒子距离 < 0.25；
     *          - 形状：距离 < 包围半径 且 体素包含测试 DropCollision 通过；
     *          - 线段：点到线段最近距离 < 0.25。
     *
     * @note 3D 相对 2D 的修正：线段分支改用 `PhysicsLine3D::GetSegment()` 取端点，
     *       不再像 2D 那样手工 `vec2angle({radius,0}, angle)` 重建（2D 依赖
     *       `radius` 恰好是半长度这一隐含约定，3D 把它显式化了）。 */
    PhysicsFormwork3D *PhysicsWorld3D::Get(Vec3_ pos)
    {
        std::vector<PhysicsFormwork3D *> SearchV;
        mGridSearch.Get(pos, 1.0f, SearchV);
        for (auto i : SearchV)
        {
            switch (i->PFGetType())
            {
            case PhysicsObjectEnum::circle:
                if (Modulus(((PhysicsCircle3D *)i)->pos - pos) < ((PhysicsCircle3D *)i)->radius)
                {
                    return ((PhysicsCircle3D *)i);
                }
                break;
            case PhysicsObjectEnum::particle:
                if (Modulus(((PhysicsParticle3D *)i)->pos - pos) < 0.25f)
                {
                    return ((PhysicsParticle3D *)i);
                }
                break;
            case PhysicsObjectEnum::shape:
                if ((Modulus(((PhysicsShape3D *)i)->pos - pos) < ((PhysicsShape3D *)i)->PFGetCollisionR()) &&
                    (((PhysicsShape3D *)i)->DropCollision(pos).Collision))
                {
                    return ((PhysicsShape3D *)i);
                }
                break;
            default:
                break;
            }
        }

        Vec3_ Drop;
        Vec3_ segBegin;
        Vec3_ segEnd;
        for (auto i : PhysicsLineS)
        {
            i->GetSegment(segBegin, segEnd);
            Drop = DropUptoLineShortesIntersect(segBegin, segEnd, pos);
            if (Modulus(Drop - pos) < 0.25f)
            {
                return ((PhysicsLine3D *)i);
            }
        }

        return nullptr;
    }

    /**
     * @brief 获取世界内的总动能
     * @return 世界总动能
     * @details 平动动能 + 转动动能之和，最后除以 2。
     *          平动：用 `ModulusLength(speed)`（**平方长度**，即 v²，故与末尾的 ½ 配套）；
     *          转动：2D 是标量 `I·ω²`，3D 惯量是张量，必须写成 `ωᵀ·I_world·ω`。
     *          由于 `MomentInertia` 存的是**局部系**张量，用
     *          `ωᵀ·(R·I_local·Rᵀ)·ω = (Rᵀω)ᵀ·I_local·(Rᵀω)` 换算，避免显式构造世界系张量。
     *          液体粒子属流体子系统，不计入刚体动能统计。 */
    FLOAT_ PhysicsWorld3D::GetWorldEnergy()
    {
        FLOAT_ Energy = 0;
        for (auto i : PhysicsShapeS)
        {
            Energy += i->mass * ModulusLength(i->speed);
            const Mat3_ R = glm::mat3_cast(i->rotation);
            const Vec3_ localOmega = glm::transpose(R) * i->angleSpeed;
            Energy += Dot(localOmega, i->MomentInertia * localOmega);
        }
        for (auto i : PhysicsParticleS)
        {
            if (i->IsLiquidParticle)
            {
                continue;
            }
            Energy += i->mass * ModulusLength(i->speed);
        }
        for (auto i : PhysicsCircleS)
        {
            Energy += i->mass * ModulusLength(i->speed);
            const Mat3_ R = glm::mat3_cast(i->rotation);
            const Vec3_ localOmega = glm::transpose(R) * i->angleSpeed;
            Energy += Dot(localOmega, i->MomentInertia * localOmega);
        }
        return Energy / 2;
    }

} // namespace PhysicsBlock3D
