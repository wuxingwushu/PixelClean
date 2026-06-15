#pragma once
#include <vector>
#include "MapFormwork.hpp"     // 地图样板
#include "PhysicsShape.hpp"    // 有形状物体
#include "PhysicsParticle.hpp" // 物理粒子
#include "PhysicsLine.hpp"     // 物理线
#include "PhysicsArbiter.hpp"  // 物理解析單元
#include "PhysicsJoint.hpp"     // 物理关节
#include "PhysicsJunction.hpp"  // 物理连接
#include "PhysicsAssembly.hpp"  // 物理组装体
#include "GridSearch.hpp"            // 网格搜索
#include "PhysicsCollision.hpp"     // 碰撞回调系统
#include "PhysicsKinematic.hpp"     // 运动学物体系统
#include "PhysicsTrigger.hpp"       // 触发器系统
#include <unordered_map>

#define MemoryPoolBool 1
#if MemoryPoolBool
#if TranslatorLocality
#include "../Tool/MemoryPool.h"
#else
#include "MemoryPool.h"
#endif
#endif

#define ThreadPoolBool 1
#if ThreadPoolBool
#include <mutex>
#if TranslatorLocality
#include "../Tool/ThreadPool.h"
#else
#include "ThreadPool.h"
#endif
#endif

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

namespace PhysicsBlock
{

class PhysicsGPU;

#if MemoryPoolBool
#if ThreadPoolBool

// 每线程独占 MemoryPool 的数组大小（8 worker + 1 main）
constexpr unsigned kMaxPoolThreads = 9;
constexpr unsigned kMainThreadPoolIndex = kMaxPoolThreads - 1;

/**
 * @brief 辅助生成创建对象内存池声明（per-thread 数组版，无锁）
 * @param Arbiter_ 处理两种物体碰撞的类
 * @param Type1 物体1类型
 * @param Name1 物体1简称
 * @param Type2 物体2类型
 * @param Name2 物体2简称 */
#define AuxiliaryMemoryPool(Arbiter_, Type1, Name1, Type2, Name2)                        \
    MemoryPool<Arbiter_, 1000 * sizeof(Arbiter_)> Pool##Arbiter_[kMaxPoolThreads];       \
    void Arbiter(Type1 *Name1, Type2 *Name2);

/**
 * @brief 辅助创建对象函数生成
 * @param Arbiter_ 处理两种物体碰撞的类
 * @param Type1 物体1类型
 * @param Name1 物体1简称
 * @param Type2 物体2类型
 * @param Name2 物体2简称
 * @param ID 唯一数字，且和销毁函数统一 */
#define AuxiliaryArbiter(Arbiter_, Type1, Name1, Type2, Name2, ID) \
    inline void PhysicsWorld::Arbiter(Type1 *Name1, Type2 *Name2)  \
    {                                                              \
        const unsigned char tId = tlCollideOutput                  \
            ? tlCollideOutput->mThreadIndex                        \
            : static_cast<unsigned char>(kMainThreadPoolIndex);    \
        BaseArbiter *ptr = Pool##Arbiter_[tId].newElement(Name1, Name2); \
        ptr->key.PoolID = static_cast<char>(ID);                  \
        ptr->mAllocThreadIndex = tId;                              \
        HandleCollideGroup(ptr);                                   \
    }

/**
 * @brief 辅助生成销毁对象的代码块
 * @param Arbiter_ 处理两种物体碰撞的类
 * @param Type1 物体1类型
 * @param Name1 物体1简称
 * @param Type2 物体2类型
 * @param Name2 物体2简称
 * @param ID 唯一数字，且和创建函数统一 */
#define AuxiliaryDelete(Arbiter_, Type1, Name1, Type2, Name2, ID) \
    case ID:                                                      \
    {                                                             \
        Pool##Arbiter_[BA->mAllocThreadIndex].deleteElement((Arbiter_ *)BA); \
    }                                                             \
    break;
#else
/**
 * @brief 辅助生成创建对象内存池声明
 * @param Arbiter_ 处理两种物体碰撞的类
 * @param Type1 物体1类型
 * @param Name1 物体1简称
 * @param Type2 物体2类型
 * @param Name2 物体2简称 */
#define AuxiliaryMemoryPool(Arbiter_, Type1, Name1, Type2, Name2)  \
    MemoryPool<Arbiter_, 10000 * sizeof(Arbiter_)> Pool##Arbiter_; \
    void Arbiter(Type1 *Name1, Type2 *Name2);

/**
 * @brief 辅助创建对象函数生成
 * @param Arbiter_ 处理两种物体碰撞的类
 * @param Type1 物体1类型
 * @param Name1 物体1简称
 * @param Type2 物体2类型
 * @param Name2 物体2简称
 * @param ID 唯一数字，且和销毁函数统一 */
#define AuxiliaryArbiter(Arbiter_, Type1, Name1, Type2, Name2, ID) \
    inline void PhysicsWorld::Arbiter(Type1 *Name1, Type2 *Name2)  \
    {                                                              \
        BaseArbiter *ptr;                                          \
        ptr = Pool##Arbiter_.newElement(Name1, Name2);             \
        ptr->key.PoolID = ID;                                      \
        HandleCollideGroup(ptr);                                   \
    }

/**
 * @brief 辅助生成销毁对象的代码块
 * @param Arbiter_ 处理两种物体碰撞的类
 * @param Type1 物体1类型
 * @param Name1 物体1简称
 * @param Type2 物体2类型
 * @param Name2 物体2简称
 * @param ID 唯一数字，且和创建函数统一 */
#define AuxiliaryDelete(Arbiter_, Type1, Name1, Type2, Name2, ID) \
    case ID:                                                      \
    {                                                             \
        Pool##Arbiter_.deleteElement((Arbiter_ *)BA);             \
    }                                                             \
    break;
#endif

#else
/**
 * @brief 辅助生成创建对象声明
 * @param Arbiter_ 处理两种物体碰撞的类
 * @param Type1 物体1类型
 * @param Name1 物体1简称
 * @param Type2 物体2类型
 * @param Name2 物体2简称 */
#define AuxiliaryMemoryPool(Arbiter_, Type1, Name1, Type2, Name2) \
    void Arbiter(Type1 *Name1, Type2 *Name2);

/**
 * @brief 辅助创建对象函数生成
 * @param Arbiter_ 处理两种物体碰撞的类
 * @param Type1 物体1类型
 * @param Name1 物体1简称
 * @param Type2 物体2类型
 * @param Name2 物体2简称
 * @param ID 唯一数字，且和销毁函数统一 */
#define AuxiliaryArbiter(Arbiter_, Type1, Name1, Type2, Name2, ID) \
    inline void PhysicsWorld::Arbiter(Type1 *Name1, Type2 *Name2)  \
    {                                                              \
        HandleCollideGroup(new Arbiter_(Name1, Name2));            \
    }
#endif

    // ========== 无锁碰撞输出缓冲区 ==========
    struct CollideOutput
    {
        unsigned char mThreadIndex = static_cast<unsigned char>(kMainThreadPoolIndex);

        std::vector<BaseArbiter *> newGroup;
        std::vector<ArbiterKey> deleteGroup;

        void clear()
        {
            newGroup.clear();
            deleteGroup.clear();
        }
    };

    struct CollideGroupEntry
    {
        BaseArbiter *arbiter = nullptr;
        int vectorIndex = -1;
    };

    /**
     * @brief 物理世界
     * @note 重力加速度， 网格风 */
    class PhysicsWorld SerializationInherit_
    {
#if PhysicsBlock_Serialization
    public:
        SerializationVirtualFunction;
        PhysicsWorld(const nlohmann::json_abi_v3_12_0::basic_json<> &data);

        unsigned int GetPtrIndex(PhysicsFormwork *ptr);
        void *GetIndexPtr(PhysicsObjectEnum Enum, unsigned int index);
#endif
    public:
        Vec2_ GravityAcceleration;           // 重力加速度
        const bool WindBool;                 // 是否开启网格风
        Vec2_ Wind{0};                       // 风
        Vec2_ *GridWind = nullptr;           // 网格风
        glm::uvec2 GridWindSize{0};          // 网格风大小
        MapFormwork *wMapFormwork = nullptr; // 地图对象
        GridSearch mGridSearch;              // 网格搜索
        Vec2_ mGridCenter{0};                // 上次重建时的网格中心
        FLOAT_ mGridRebuildThreshold = 64.0f; // 网格重建距离阈值
        Vec2_ mPendingGridCenter{0};         // 待应用的网格中心
        bool mGridPositionDirty = false;     // 是否有待应用的网格位置更新

        std::vector<PhysicsShape *> PhysicsShapeS;       // 物理形状网格
        std::vector<PhysicsParticle *> PhysicsParticleS; // 物理点
        std::vector<PhysicsJoint *> PhysicsJointS;       // 物理关节
        std::vector<BaseJunction *> BaseJunctionS;       // 物理绳子
        std::vector<PhysicsCircle *> PhysicsCircleS;     // 物理圆
        std::vector<PhysicsLine *> PhysicsLineS;         // 物理线
        std::vector<PhysicsAssembly *> PhysicsAssemblyS; // 物理组装体

        std::unordered_map<ArbiterKey, CollideGroupEntry, ArbiterKeyHash> CollideGroupS; // 碰撞对-键值容器（含索引）
        /**
         * @brief 碰撞对数组
         * @details 以线性数组形式存储所有活跃的碰撞对，支持 O(1) 遍历。
         *          配合 CollideGroupS 中的 vectorIndex 索引，实现 O(1) 的删除操作
         *          （将末尾元素交换到被删除位置）。 */
        std::vector<BaseArbiter *> CollideGroupVector;
        std::vector<BaseArbiter *> NewCollideGroup;                                  // 新添加的碰撞对
        std::vector<ArbiterKey> DeleteCollideGroup;                                  // 删除的碰撞对

        /**
         * @brief 碰撞回调管理器
         * @details 管理所有碰撞对的回调注册与分发，在每帧物理模拟结束后触发用户注册的碰撞回调函数。 */
        PhysicsCollision mCollision;
        /**
         * @brief 运动学物体管理器
         * @details 负责管理运动学物体（不受物理模拟影响、由用户直接控制位置的物体），
         *          每帧根据用户设定的速度更新其位置。 */
        PhysicsKinematic mKinematic;
        /**
         * @brief 触发器管理器
         * @details 管理所有触发器区域，检测物体进入/离开触发器的事件，
         *          并在每帧物理模拟结束后触发相应的回调。 */
        PhysicsTrigger mTrigger;

        // GPU 计算后端
        PhysicsGPU* mGPU = nullptr;            // GPU 物理求解器
        bool mUseGPUApplyImpulse = false;      // 运行时 GPU 开关

        void SetGPU(PhysicsGPU* gpu) { mGPU = gpu; }
        void SetUseGPUApplyImpulse(bool use) { mUseGPUApplyImpulse = use; }
        bool IsUseGPUApplyImpulse() const { return mUseGPUApplyImpulse; }

        // CPU 各阶段耗时统计 (ms)
        float mCollisionDetectionTimeMS = 0.0f;
        float mPreStepTimeMS           = 0.0f;
        float mApplyImpulseCPUTimeMS   = 0.0f;
        float mPositionUpdateTimeMS    = 0.0f;
        float mPostProcessTimeMS       = 0.0f;

        float GetCollisionDetectionTimeMS() const { return mCollisionDetectionTimeMS; }
        float GetPreStepTimeMS()           const { return mPreStepTimeMS; }
        float GetApplyImpulseCPUTimeMS()   const { return mApplyImpulseCPUTimeMS; }
        float GetPositionUpdateTimeMS()    const { return mPositionUpdateTimeMS; }
        float GetPostProcessTimeMS()       const { return mPostProcessTimeMS; }

        unsigned int ObjectSize = 0;                             // 动态物理对象总数量
        unsigned int ApplyImpulseSize = PhysicsApplyImpulseSize; // 迭代次数

        /**
         * @brief 处理碰撞对
         * @param Ba 碰撞对指针
         * @details 计算碰撞结果，将碰撞对插入或更新到 CollideGroupS 中。
         *          若有碰撞接触点则保留或更新碰撞对，若无碰撞则标记为待删除。 */
        void HandleCollideGroup(BaseArbiter *Ba);
        /**
         * @brief 解析碰撞对变更
         * @details 将 NewCollideGroup 和 DeleteCollideGroup 中缓存的添加/删除操作
         *          实际合并到 CollideGroupS 和 CollideGroupVector 中，
         *          同时同步更新碰撞回调管理器和触发器管理器。 */
        void ResolveCollideGroup();
        /**
         * @brief 合并多线程碰撞输出
         * @param outputs 各线程的碰撞输出缓冲区
         * @details 将多线程并行碰撞检测产生的 per-thread 输出合并到全局新增/删除队列中。 */
        void MergeCollideOutputs(std::vector<CollideOutput> &outputs);

        AuxiliaryMemoryPool(PhysicsArbiterCP, PhysicsCircle, C, PhysicsParticle, P);
        AuxiliaryMemoryPool(PhysicsArbiterSP, PhysicsShape, S, PhysicsParticle, P);
        AuxiliaryMemoryPool(PhysicsArbiterSS, PhysicsShape, S1, PhysicsShape, S2);
        AuxiliaryMemoryPool(PhysicsArbiterS, PhysicsShape, S, MapFormwork, M);
        AuxiliaryMemoryPool(PhysicsArbiterP, PhysicsParticle, P, MapFormwork, M);
        AuxiliaryMemoryPool(PhysicsArbiterC, PhysicsCircle, C, MapFormwork, M);
        AuxiliaryMemoryPool(PhysicsArbiterCS, PhysicsCircle, C, PhysicsShape, S);
        AuxiliaryMemoryPool(PhysicsArbiterCC, PhysicsCircle, C1, PhysicsCircle, C2);
        AuxiliaryMemoryPool(PhysicsArbiterLC, PhysicsLine, L, PhysicsCircle, C);
        AuxiliaryMemoryPool(PhysicsArbiterLS, PhysicsLine, L, PhysicsShape, S);
        AuxiliaryMemoryPool(PhysicsArbiterLP, PhysicsLine, L, PhysicsParticle, P);
        AuxiliaryMemoryPool(PhysicsArbiterL, PhysicsLine, L, MapFormwork, M);

        void DeleteArbiter(BaseArbiter *BA);

#if ThreadPoolBool
        ThreadPool mThreadPool;
        std::vector<std::future<void>> xTn;

        // 无锁碰撞输出：每个工作线程写入自己的缓冲区
        static thread_local CollideOutput *tlCollideOutput;

        // 每帧预分配的 per-thread 输出缓冲区
        std::vector<CollideOutput> mCollideOutputs;
#endif
    public:
        /**
         * @brief 构建函数
         * @param GravityAcceleration 重力加速度
         * @param Wind 风是否开启 */
        PhysicsWorld(Vec2_ GravityAcceleration, const bool Wind);
        ~PhysicsWorld();

        // 计算最小迭代次数
        void ApplyImpulseAdd()
        {
            ++ObjectSize;
            // 不可以小于最小迭代次数
            if (ObjectSize < (PhysicsApplyImpulseSize * PhysicsApplyImpulseSize * 5))
            {
                ApplyImpulseSize = PhysicsApplyImpulseSize;
            }
            else
            {
                // 这个算法是通过实验得来的。可以得到一个相对较小的迭代次数
                ApplyImpulseSize = sqrt(ObjectSize / 5);
            }
        }

        /**
         * @brief 添加形状物体到物理世界
         * @param Object 形状物体指针
         * @details 将物体注册到网格搜索，并加入物理形状列表。 */
        void AddObject(PhysicsShape *Object)
        {
            ApplyImpulseAdd();
            mGridSearch.Add(Object);
            PhysicsShapeS.push_back(Object);
        }

        /**
         * @brief 添加粒子到物理世界
         * @param Object 粒子指针
         * @details 将粒子注册到网格搜索，并加入物理粒子列表。 */
        void AddObject(PhysicsParticle *Object)
        {
            ApplyImpulseAdd();
            mGridSearch.Add(Object);
            PhysicsParticleS.push_back(Object);
        }

        /**
         * @brief 添加关节到物理世界
         * @param Object 关节指针
         * @details 将关节加入物理关节列表，关节用于约束两个物体之间的相对运动。 */
        void AddObject(PhysicsJoint *Object)
        {
            ApplyImpulseAdd();
            PhysicsJointS.push_back(Object);
        }

        /**
         * @brief 添加连接体到物理世界
         * @param Object 连接体指针
         * @details 将连接体（绳子/弹簧）加入物理连接列表。 */
        void AddObject(BaseJunction *Object)
        {
            ApplyImpulseAdd();
            BaseJunctionS.push_back(Object);
        }

        /**
         * @brief 添加圆形物体到物理世界
         * @param Object 圆形物体指针
         * @details 将圆形物体注册到网格搜索，并加入物理圆列表。 */
        void AddObject(PhysicsCircle *Object)
        {
            ApplyImpulseAdd();
            mGridSearch.Add(Object);
            PhysicsCircleS.push_back(Object);
        }

        /**
         * @brief 添加线段到物理世界
         * @param Object 线段指针
         * @details 将线段加入物理线列表。 */
        void AddObject(PhysicsLine *Object)
        {
            ApplyImpulseAdd();
            PhysicsLineS.push_back(Object);
        }

        /**
         * @brief 添加组装体到物理世界
         * @param Object 组装体指针
         * @details 调用组装体的 AddToWorld 将其所有子对象批量添加到物理世界，
         *          然后将组装体本身加入组装体列表。 */
        void AddObject(PhysicsAssembly *Object)
        {
            Object->AddToWorld(this);
            PhysicsAssemblyS.push_back(Object);
        }

        /**
         * @brief 从物理世界中移除物体
         * @param Object 待移除的物理物体指针
         * @details 根据物体类型从对应的列表中移除，同时清理关联的关节、连接体、碰撞对等。 */
        void RemoveObject(PhysicsFormwork *Object);
        /**
         * @brief 从物理世界中移除组装体
         * @param Object 待移除的组装体指针
         * @details 调用组装体的 RemoveFromWorld 将其所有子对象从物理世界中移除，
         *          然后从组装体列表中移除并销毁组装体。 */
        void RemoveObject(PhysicsAssembly *Object);

        std::vector<PhysicsShape *> &GetPhysicsShape()
        {
            return PhysicsShapeS;
        }

        std::vector<PhysicsParticle *> &GetPhysicsParticle()
        {
            return PhysicsParticleS;
        }

        std::vector<PhysicsJoint *> &GetPhysicsJoint()
        {
            return PhysicsJointS;
        }

        std::vector<BaseJunction *> &GetBaseJunction()
        {
            return BaseJunctionS;
        }

        std::vector<PhysicsCircle *> &GetPhysicsCircle()
        {
            return PhysicsCircleS;
        }

        std::vector<PhysicsLine *> &GetPhysicsLine()
        {
            return PhysicsLineS;
        }

        std::vector<PhysicsAssembly *> &GetPhysicsAssembly()
        {
            return PhysicsAssemblyS;
        }

        /**
         * @brief 获取当前地图对象
         * @return 地图对象指针，可能为 nullptr */
        MapFormwork *GetMapFormwork()
        {
            return wMapFormwork;
        }

        /**
         * @brief 获取碰撞回调管理器
         * @return 碰撞回调管理器的引用 */
        PhysicsCollision &GetCollision()
        {
            return mCollision;
        }

        /**
         * @brief 获取位置上的对象
         * @param pos 位置
         * @return 对象指针
         * @retval nullptr 没有对象 */
        PhysicsFormwork *Get(Vec2_ pos);

        /**
         * @brief 检查两个物体是否属于同一个组装体
         * @param a 第一个物体
         * @param b 第二个物体
         * @return 如果属于同一个非空组装体返回 true */
        inline static bool SameAssembly(const PhysicsFormwork *a, const PhysicsFormwork *b)
        {
            return a->assembly && a->assembly == b->assembly;
        }

        /**
         * @brief   设置地图
         * @param   MapFormwork_ 地图指针
         * @details 设置世界的地形碰撞对象，同时初始化网格风大小和网格搜索范围。 */
        void SetMapFormwork(MapFormwork *MapFormwork_);

        /**
         * @brief   根据焦点位置更新网格位置
         * @param   focusPoint 焦点位置（如摄像机中心），世界坐标
         * @details 仅当焦点与上次重建中心距离超过阈值时才触发网格重建
         */
        void UpdateGridPosition(Vec2_ focusPoint);

        /**
         * @brief   设置网格重建距离阈值
         * @param   threshold 距离阈值，焦点移动超过此距离才触发网格重建
         */
        void SetGridRebuildThreshold(FLOAT_ threshold) { mGridRebuildThreshold = threshold; }

        /**
         * @brief 物理仿真主循环
         * @param time 时间步长
         * @details 执行一帧完整的物理模拟，包括：碰撞检测、预处理（PreStep）、
         *          冲量迭代求解（ApplyImpulse）、位置更新（PhysicsPos）、
         *          碰撞回调处理、触发器处理、网格搜索更新。支持多线程并行加速。 */
        void PhysicsEmulator(FLOAT_ time);

        /**
         * @brief 物理信息更新
         * @details 执行碰撞检测和预处理，更新网格搜索树，但不执行冲量求解和位置更新。
         *          通常用于无需完整物理模拟帧的场景（如序列化恢复、静态场景初始化）。 */
        void PhysicsInformationUpdate();

        /**
         * @brief 等待碰撞检测线程完成
         * @details 阻塞等待所有正在运行的碰撞检测线程任务结束。 */
        void WaitForCollisionThreads();

        /**
         * @brief 获取世界内的能量
         * @return 能量 */
        FLOAT_ GetWorldEnergy();
    };

}