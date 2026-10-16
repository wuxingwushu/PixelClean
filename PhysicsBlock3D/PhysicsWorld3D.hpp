#pragma once
#include <vector>
#include <chrono>
#include <algorithm>
#include <cmath>
#include "BaseDefine3D.h"          // 3D 总开关与常量
#include "MapFormwork3D.hpp"       // 地图样板
#include "PhysicsShape3D.hpp"      // 有形状物体
#include "PhysicsParticle3D.hpp"   // 物理粒子
#include "PhysicsLine3D.hpp"       // 物理线
#include "PhysicsArbiter3D.hpp"    // 物理解析单元
#include "PhysicsBaseCollide3D.hpp" // 宽相 CollideAABB / 窄相 Collide（自由函数）
#include "PhysicsJoint3D.hpp"      // 物理关节
#include "PhysicsJunction3D.hpp"   // 物理连接
#include "PhysicsAssembly3D.hpp"   // 物理组装体
#include "GridSearch3D.hpp"        // 网格搜索
#include "PhysicsCollision3D.hpp"  // 碰撞回调系统
#include "PhysicsKinematic3D.hpp"  // 运动学物体系统
#include "PhysicsTrigger3D.hpp"    // 触发器系统
#include <unordered_map>
#include <thread>
#include <future>

// 内存池与线程池的头文件必须包含在命名空间之外（它们会引入 libc++ 标准库头）
#define MemoryPoolBool3D 1
#if MemoryPoolBool3D
// 注意：这里**不能**用 ../Tool/MemoryPool.h（2D 版有 freeSlots/freeSlots_ 笔误，
// 会导致任何实例化 MemoryPool 的翻译单元编译失败）；3D 版 MemoryPool3D.h 已修正。
#include "MemoryPool3D.h"
#endif

#define ThreadPoolBool3D 1
#if ThreadPoolBool3D
#include <mutex>
#if TranslatorLocality3D
#include "../Tool/ThreadPool.h"
#else
#include "ThreadPool3D.h"
#endif
#endif

namespace PhysicsBlock3D
{

/**
 * @brief 线性索引切分（3D 版，与 2D 完全同构）
 * @details 把 [0, Size) 按线程编号 Tn（0-based）与线程总数 TSize 切成不重叠区间 [S, E)。
 *          纯整数运算，与物理维度无关，故 3D 原样保留。
 *          2D 在 PhysicsWorld.hpp:37-58 定义，本处仅改名加 3D 后缀避免宏冲突。 */
#define ThreadTaskAllot3D(S, E, Size, Tn, TSize) \
    if (Tn == TSize)                             \
    {                                            \
        S = 0;                                   \
        E = Size;                                \
    }                                            \
    else                                         \
    {                                            \
        S = Size / TSize;                        \
        E = Size % TSize;                        \
        if ((int)Tn < (int)E)                    \
        {                                        \
            E = S * Tn + Tn + S + 1;             \
            S = S * Tn + Tn;                     \
        }                                        \
        else                                     \
        {                                        \
            int Y = E;                           \
            E = S * Tn + Y + S;                  \
            S = S * Tn + Y;                      \
        }                                        \
    }

    // kMaxPoolThreads3D / kMainThreadPoolIndex3D 定义在 BaseArbiter3D.hpp

/**
 * @brief 为某种裁决器生成 per-thread 内存池 + Arbiter 工厂声明（3D 版）
 * @param Arbiter_ 裁决器具体类型
 * @param Type1/Name1 第一类物体类型与形参名
 * @param Type2/Name2 第二类物体类型与形参名
 * @details 与 2D 的三分支结构完全同构，只是类型名全部换成 3D 版。
 *          注意 3D 的 Contact3D 比 2D 的 Contact 大（Vec3_ 替代 Vec2_、多一个切向冲量），
 *          故 1000*sizeof(Arbiter_) 的单块字节数会显著增长。 */
#define AuxiliaryMemoryPool3D(Arbiter_, Type1, Name1, Type2, Name2)                      \
    MemoryPool<Arbiter_, 1000 * sizeof(Arbiter_)> Pool##Arbiter_[kMaxPoolThreads3D];     \
    void Arbiter(Type1 *Name1, Type2 *Name2);

/**
 * @brief Arbiter 工厂实现（内存池 + 线程池分支，3D 版） */
#define AuxiliaryArbiter3D(Arbiter_, Type1, Name1, Type2, Name2, ID) \
    inline void PhysicsWorld3D::Arbiter(Type1 *Name1, Type2 *Name2)  \
    {                                                                \
        const unsigned char tId =                                    \
            tlCollideOutput ? tlCollideOutput->mThreadIndex : (unsigned char)kMainThreadPoolIndex3D; \
        BaseArbiter3D *ptr = Pool##Arbiter_[tId].newElement(Name1, Name2); \
        ptr->key.PoolID = (char)ID;                                  \
        ptr->mAllocThreadIndex = tId;                                \
        HandleCollideGroup(ptr);                                     \
    }

/**
 * @brief Arbiter 回收（3D 版） */
#define AuxiliaryDelete3D(Arbiter_, Type1, Name1, Type2, Name2, ID) \
    case ID:                                                        \
        Pool##Arbiter_[BA->mAllocThreadIndex].deleteElement((Arbiter_ *)BA); \
        break;

    /**
     * @brief 物理世界（3D）
     * @details 由 2D PhysicsWorld 逐类改写。九阶段仿真骨架、线程池、
     *          裁决器生命周期、内存池宏全部与维度无关，原样保留；
     *          真正改的是类型层（Vec3_/Quat_/Mat3_）、空间索引（GridSearch3D）
     *          与窄相（PhysicsBaseCollide3D）。
     *
     *          相对 2D 的修正：
     *          ① `GridWind` 用 `delete[]` 释放（2D 是 `new[]`/`delete` 不匹配的 UB）；
     *          ② 移除 `PhysicsGPU` 段（3D 不含 GPU 后端，见 BaseDefine3D.h 说明）；
     *          ③ `Get(Vec3_)` 的线段分支改用 `GetSegment()`，不再手工 vec2angle 重建端点；
     *          ④ `GetWorldEnergy()` 的转动项改用 `½ω·(Iω)`（三维惯量张量），
     *             而非 2D 的标量 `I·ω²`。
     */
    class PhysicsWorld3D
    {
    public:
        /** @brief 重力加速度（3D 向量） */
        Vec3_ GravityAcceleration;
        /** @brief 风场开关（构造后不可变；风场本身为预留特性） */
        const bool WindBool;
        /** @brief 全局风向量 */
        Vec3_ Wind{0.0f};
        /** @brief 逐格风场（尺寸 = GridWindSize；WindBool 为真时才分配） */
        Vec3_ *GridWind = nullptr;
        /** @brief 逐格风场三维尺寸 */
        UVec3_ GridWindSize{0};
        /** @brief 当前绑定的地图（可为空） */
        MapFormwork3D *wMapFormwork = nullptr;
        /** @brief 空间索引 */
        GridSearch3D mGridSearch;
        /** @brief 网格中心（用于判定是否需要平移网格） */
        Vec3_ mGridCenter{0.0f};
        /** @brief 网格重建阈值 */
        FLOAT_ mGridRebuildThreshold = 64.0f;
        /** @brief 待应用的网格中心 */
        Vec3_ mPendingGridCenter{0.0f};
        /** @brief 网格位置是否待更新 */
        bool mGridPositionDirty = false;

        /** @brief 形状物体集合 */
        std::vector<PhysicsShape3D *> PhysicsShapeS;
        /** @brief 粒子集合 */
        std::vector<PhysicsParticle3D *> PhysicsParticleS;
        /** @brief 关节集合 */
        std::vector<PhysicsJoint3D *> PhysicsJointS;
        /** @brief 连接（绳/弹簧/杠杆/橡皮）集合 */
        std::vector<BaseJunction3D *> BaseJunctionS;
        /** @brief 圆球集合 */
        std::vector<PhysicsCircle3D *> PhysicsCircleS;
        /** @brief 线段集合（不入网格宽相） */
        std::vector<PhysicsLine3D *> PhysicsLineS;
        /** @brief 组装体集合 */
        std::vector<PhysicsAssembly3D *> PhysicsAssemblyS;

        /** @brief 碰撞对表：key → {裁决器, 在 CollideGroupVector 中的下标} */
        std::unordered_map<ArbiterKey3D, CollideGroupEntry3D, ArbiterKey3DHash> CollideGroupS;
        /** @brief 活跃裁决器线性表（便于无哈希迭代与稳定索引） */
        std::vector<BaseArbiter3D *> CollideGroupVector;
        /** @brief 本帧新增的裁决器（单线程路径用） */
        std::vector<BaseArbiter3D *> NewCollideGroup;
        /** @brief 本帧待删除的裁决器键（单线程路径用） */
        std::vector<ArbiterKey3D> DeleteCollideGroup;

        /**
         * @brief 本帧内用于标记"该碰撞对已消失"的删除宏
         * @details 只有已经登记在 CollideGroupS 里的对才需要删除；
         *          未登记的对直接丢弃即可（否则会在删除阶段空转）。 */
#define Map_Delete3D(key)                                                       \
    do                                                                          \
    {                                                                           \
        if (CollideGroupS.find(key) != CollideGroupS.end())                     \
        {                                                                       \
            if (tlCollideOutput)                                                \
                tlCollideOutput->deleteGroup.push_back(key);                    \
            else                                                                \
                DeleteCollideGroup.push_back(key);                              \
        }                                                                       \
    } while (0)

        /**
         * @brief 碰撞回调管理器 */
        PhysicsCollision3D mCollision;
        /**
         * @brief 运动学物体管理器 */
        PhysicsKinematic3D mKinematic;
        /**
         * @brief 触发器管理器 */
        PhysicsTrigger3D mTrigger;

        // 液体模拟（可选；Phase 12 落地后把 PhysicsLiquid3DBool3D 置 1）
#if PhysicsLiquid3DBool3D
        class PhysicsLiquid3D *mLiquid = nullptr;
        void SetLiquid(class PhysicsLiquid3D *liquid) { mLiquid = liquid; }
        class PhysicsLiquid3D *GetLiquid() const { return mLiquid; }
#endif

        // CPU 各阶段耗时统计 (ms)
        float mCollisionDetectionTimeMS = 0.0f;
        float mPreStepTimeMS = 0.0f;
        float mApplyImpulseCPUTimeMS = 0.0f;
        float mPositionUpdateTimeMS = 0.0f;
        float mPostProcessTimeMS = 0.0f;

        float GetCollisionDetectionTimeMS() const { return mCollisionDetectionTimeMS; }
        float GetPreStepTimeMS() const { return mPreStepTimeMS; }
        float GetApplyImpulseCPUTimeMS() const { return mApplyImpulseCPUTimeMS; }
        float GetPositionUpdateTimeMS() const { return mPositionUpdateTimeMS; }
        float GetPostProcessTimeMS() const { return mPostProcessTimeMS; }

        /** @brief 动态物理对象总数量 */
        unsigned int ObjectSize = 0;
        /** @brief 冲量迭代次数（随物体数自适应） */
        unsigned int ApplyImpulseSize = PhysicsApplyImpulseSize3D;

        /**
         * @brief 新物体加入时更新迭代次数
         * @details 物体少时用固定迭代数；物体多时按 sqrt(ObjectSize/5) 增长，
         *          在解算质量与耗时之间折中。3D 与 2D 判据一致。 */
        inline void ApplyImpulseAdd()
        {
            ++ObjectSize;
            if (ObjectSize < (PhysicsApplyImpulseSize3D * PhysicsApplyImpulseSize3D * 5))
            {
                ApplyImpulseSize = PhysicsApplyImpulseSize3D;
            }
            else
            {
                ApplyImpulseSize = (unsigned int)sqrt(ObjectSize / 5);
            }
        }

        // ==================== 对象增删 ====================

        /** @brief 添加形状物体 */
        inline void AddObject(PhysicsShape3D *Object)
        {
            ApplyImpulseAdd();
            mGridSearch.Add(Object);
            PhysicsShapeS.push_back(Object);
        }
        /** @brief 添加粒子 */
        inline void AddObject(PhysicsParticle3D *Object)
        {
            ApplyImpulseAdd();
            mGridSearch.Add(Object);
            PhysicsParticleS.push_back(Object);
        }
        /** @brief 添加圆球 */
        inline void AddObject(PhysicsCircle3D *Object)
        {
            ApplyImpulseAdd();
            mGridSearch.Add(Object);
            PhysicsCircleS.push_back(Object);
        }
        /** @brief 添加线段（不入网格宽相，与 2D 一致） */
        inline void AddObject(PhysicsLine3D *Object)
        {
            ApplyImpulseAdd();
            PhysicsLineS.push_back(Object);
        }
        /** @brief 添加关节 */
        inline void AddObject(PhysicsJoint3D *Object)
        {
            ApplyImpulseAdd();
            PhysicsJointS.push_back(Object);
        }
        /** @brief 添加连接 */
        inline void AddObject(BaseJunction3D *Object)
        {
            ApplyImpulseAdd();
            BaseJunctionS.push_back(Object);
        }
        /** @brief 添加组装体（内部子体一并入世界） */
        inline void AddObject(PhysicsAssembly3D *Object)
        {
            Object->AddToWorld(this);
            PhysicsAssemblyS.push_back(Object);
        }

        /** @brief 移除并销毁一个物理物体（连带清理关节/连接/碰撞对） */
        void RemoveObject(PhysicsFormwork3D *Object);
        /** @brief 移除并销毁一个组装体（连带其全部子体） */
        void RemoveObject(PhysicsAssembly3D *Object);

        // ==================== 访问器 ====================

        std::vector<PhysicsShape3D *> &GetPhysicsShapeS() { return PhysicsShapeS; }
        std::vector<PhysicsParticle3D *> &GetPhysicsParticleS() { return PhysicsParticleS; }
        std::vector<PhysicsJoint3D *> &GetPhysicsJointS() { return PhysicsJointS; }
        std::vector<BaseJunction3D *> &GetBaseJunctionS() { return BaseJunctionS; }
        std::vector<PhysicsCircle3D *> &GetPhysicsCircleS() { return PhysicsCircleS; }
        std::vector<PhysicsLine3D *> &GetPhysicsLineS() { return PhysicsLineS; }
        std::vector<PhysicsAssembly3D *> &GetPhysicsAssemblyS() { return PhysicsAssemblyS; }
        MapFormwork3D *GetMapFormwork() const { return wMapFormwork; }
        PhysicsCollision3D &GetCollision() { return mCollision; }
        PhysicsKinematic3D &GetKinematic() { return mKinematic; }
        PhysicsTrigger3D &GetTrigger() { return mTrigger; }

        /**
         * @brief 判断两个物体是否属于同一组装体（同组装体内成员互不碰撞）
         * @details 纯指针身份比较，与维度无关；3D 原样保留 2D 语义。
         *          注意 2D 版不检查 null，本版补上（否则传 null 会崩）。 */
        inline static bool SameAssembly(const PhysicsFormwork3D *a, const PhysicsFormwork3D *b)
        {
            return a && b && a->assembly && a->assembly == b->assembly;
        }

        /**
         * @brief 获取指定位置上的物理对象（射线点选）
         * @param pos 查询位置（世界坐标）
         * @return 命中的物体，未命中返回 nullptr
         * @details 先网格粗筛，再精确判定：
         *          圆球→点到球心距离 < 半径；粒子→距离 < 0.25；
         *          形状→距离 < 包围半径 且 体素包含测试通过；
         *          线段→点到线段最近距离 < 0.25（3D 用 `GetSegment()` 取端点，
         *          不再像 2D 那样手工 vec2angle 重建）。 */
        PhysicsFormwork3D *Get(Vec3_ pos);

        // ==================== 地图与网格 ====================

        /** @brief 绑定地图（注入碰撞变更回调桥接、按地图尺寸重设网格范围） */
        void SetMapFormwork(MapFormwork3D *MapFormwork_);
        /** @brief 按焦点位置决定是否平移空间网格 */
        void UpdateGridPosition(Vec3_ focusPoint);
        /** @brief 设置网格重建阈值 */
        void SetGridRebuildThreshold(FLOAT_ threshold) { mGridRebuildThreshold = threshold; }

        // ==================== 仿真 ====================

        /** @brief 完整物理仿真一帧（九阶段） */
        void PhysicsEmulator(FLOAT_ time);
        /** @brief 仅更新碰撞信息（不积分、不解算、不移动） */
        void PhysicsInformationUpdate();
        /** @brief 等待上一帧异步发起的碰撞检测任务结束 */
        void WaitForCollisionThreads();
        /** @brief 世界总动能（平动 + 转动） */
        FLOAT_ GetWorldEnergy();

        /** @brief 构造（指定重力与风场开关） */
        PhysicsWorld3D(Vec3_ gravityAcceleration, const bool wind);
        /** @brief 析构（级联释放所有持有对象） */
        ~PhysicsWorld3D();

#if PhysicsBlock_Serialization3D
        PhysicsWorld3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data);
        void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data);
        void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data);
        unsigned int GetPtrIndex(PhysicsFormwork3D *ptr);
        void *GetIndexPtr(PhysicsObjectEnum Enum, unsigned int index);
#endif

    private:
        /**
         * @brief 处理一个临时裁决器
         * @details 调用 ComputeCollide()，再按结果决定：
         *          - 有接触点且未登记 → 加入新增队列（**不回收**，交由 Resolve 接管）；
         *          - 有接触点且已登记 → 用新接触点热启动更新已有裁决器；
         *          - 无接触点且已登记 → 标记删除。
         *          最后回收临时裁决器。keep 规则 = `numContacts > 0`。 */
        void HandleCollideGroup(BaseArbiter3D *Ba);
        /** @brief 把新增/删除队列实际应用到 CollideGroupS 与 CollideGroupVector */
        void ResolveCollideGroup();
        /** @brief 合并多线程碰撞输出（预留给外部合并调用） */
        void MergeCollideOutputs(std::vector<CollideOutput3D> &outputs);

        AuxiliaryMemoryPool3D(PhysicsArbiterCP3D, PhysicsCircle3D, C, PhysicsParticle3D, P)
        AuxiliaryMemoryPool3D(PhysicsArbiterSP3D, PhysicsShape3D, S, PhysicsParticle3D, P)
        AuxiliaryMemoryPool3D(PhysicsArbiterSS3D, PhysicsShape3D, S1, PhysicsShape3D, S2)
        AuxiliaryMemoryPool3D(PhysicsArbiterS3D, PhysicsShape3D, S, MapFormwork3D, M)
        AuxiliaryMemoryPool3D(PhysicsArbiterP3D, PhysicsParticle3D, P, MapFormwork3D, M)
        AuxiliaryMemoryPool3D(PhysicsArbiterC3D, PhysicsCircle3D, C, MapFormwork3D, M)
        AuxiliaryMemoryPool3D(PhysicsArbiterCS3D, PhysicsCircle3D, C, PhysicsShape3D, S)
        AuxiliaryMemoryPool3D(PhysicsArbiterCC3D, PhysicsCircle3D, C1, PhysicsCircle3D, C2)
        AuxiliaryMemoryPool3D(PhysicsArbiterLC3D, PhysicsLine3D, L, PhysicsCircle3D, C)
        AuxiliaryMemoryPool3D(PhysicsArbiterLS3D, PhysicsLine3D, L, PhysicsShape3D, S)
        AuxiliaryMemoryPool3D(PhysicsArbiterLP3D, PhysicsLine3D, L, PhysicsParticle3D, P)
        AuxiliaryMemoryPool3D(PhysicsArbiterL3D, PhysicsLine3D, L, MapFormwork3D, M)

        /** @brief 回收一个裁决器（按 key.PoolID 分派到对应内存池） */
        void DeleteArbiter(BaseArbiter3D *BA);

#if ThreadPoolBool3D
        ThreadPool mThreadPool;
        std::vector<std::future<void>> xTn;

        // 无锁碰撞输出：每个工作线程写入自己的缓冲区
        static thread_local CollideOutput3D *tlCollideOutput;

        // 每帧预分配的 per-thread 输出缓冲区
        std::vector<CollideOutput3D> mCollideOutputs;
#endif
    };

} // namespace PhysicsBlock3D
