#pragma once
#include <vector>
#include "MapFormwork.hpp"     // 地图样板
#include "PhysicsShape.hpp"    // 有形状物体
#include "PhysicsParticle.hpp" // 物理粒子
#include "PhysicsLine.hpp"     // 物理线
#include "PhysicsArbiter.hpp"  // 物理解析單元
#include "PhysicsJoint.hpp"    // 物理关节
#include "PhysicsJunction.hpp" // 物理连接
#include "GridSearch.hpp"      // 网格搜索
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

namespace PhysicsBlock
{

#if MemoryPoolBool
#if ThreadPoolBool

/**
 * @brief 辅助生成创建对象内存池声明
 * @param Arbiter_ 处理两种物体碰撞的类
 * @param Type1 物体1类型
 * @param Name1 物体1简称
 * @param Type2 物体2类型
 * @param Name2 物体2简称 */
#define AuxiliaryMemoryPool(Arbiter_, Type1, Name1, Type2, Name2) \
    std::mutex mLock##Name1##Name2;                               \
    MemoryPool<Arbiter_, 1000 * sizeof(Arbiter_)> Pool##Arbiter_; \
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
        mLock##Name1##Name2.lock();                                \
        ptr = Pool##Arbiter_.newElement(Name1, Name2);             \
        mLock##Name1##Name2.unlock();                              \
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
        std::unique_lock<std::mutex> lock(mLock##Name1##Name2);   \
        Pool##Arbiter_.deleteElement((Arbiter_ *)BA);             \
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

    /**
     * @brief 物理世界
     * @note 重力加速度， 网格风 */
    class PhysicsWorld SerializationInherit_
    {
#if PhysicsBlock_Serialization
    public:
        SerializationVirtualFunction;
        PhysicsWorld(const nlohmann::json_abi_v3_12_0::basic_json<> &data);

        unsigned int GetPtrIndex(PhysicsFormwork* ptr);
        void* GetIndexPtr(PhysicsObjectEnum Enum, unsigned int index);
#endif
    public:
        Vec2_ GravityAcceleration;           // 重力加速度
        const bool WindBool;                 // 是否开启网格风
        Vec2_ Wind{0};                       // 风
        Vec2_ *GridWind = nullptr;           // 网格风
        glm::uvec2 GridWindSize{0};          // 网格风大小
        MapFormwork *wMapFormwork = nullptr; // 地图对象
        GridSearch mGridSearch;              // 网格搜索

        std::vector<PhysicsShape *> PhysicsShapeS;       // 物理形状网格
        std::vector<PhysicsParticle *> PhysicsParticleS; // 物理点
        std::vector<PhysicsJoint *> PhysicsJointS;       // 物理关节
        std::vector<BaseJunction *> BaseJunctionS;       // 物理绳子
        std::vector<PhysicsCircle *> PhysicsCircleS;     // 物理圆
        std::vector<PhysicsLine *> PhysicsLineS;         // 物理圆

        std::unordered_map<ArbiterKey, BaseArbiter *, ArbiterKeyHash> CollideGroupS; // 碰撞队
        std::vector<BaseArbiter *> CollideGroupVector;
        std::vector<BaseArbiter *> NewCollideGroup;
        std::vector<ArbiterKey> DeleteCollideGroup;

        void HandleCollideGroup(BaseArbiter *Ba);

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
        std::mutex mLock;
        ThreadPool mThreadPool;
        std::vector<std::future<void>> xTn;
#endif
    public:
        /**
         * @brief 构建函数
         * @param GravityAcceleration 重力加速度
         * @param Wind 风是否开启 */
        PhysicsWorld(Vec2_ GravityAcceleration, const bool Wind);
        ~PhysicsWorld();

        void AddObject(PhysicsShape *Object)
        {
            mGridSearch.Add(Object);
            PhysicsShapeS.push_back(Object);
        }

        void AddObject(PhysicsParticle *Object)
        {
            mGridSearch.Add(Object);
            PhysicsParticleS.push_back(Object);
        }

        void AddObject(PhysicsJoint *Object)
        {
            PhysicsJointS.push_back(Object);
        }

        void AddObject(BaseJunction *Object)
        {
            BaseJunctionS.push_back(Object);
        }

        void AddObject(PhysicsCircle *Object)
        {
            mGridSearch.Add(Object);
            PhysicsCircleS.push_back(Object);
        }

        void AddObject(PhysicsLine *Object)
        {
            PhysicsLineS.push_back(Object);
        }

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

        MapFormwork *GetMapFormwork()
        {
            return wMapFormwork;
        }

        /**
         * @brief 获取位置上的对象
         * @param pos 位置
         * @return 对象指针
         * @retval nullptr 没有对象 */
        PhysicsFormwork *Get(Vec2_ pos);

        /**
         * @brief 设置地图
         * @param MapFormwork_ 地图指针 */
        void SetMapFormwork(MapFormwork *MapFormwork_);

        /**
         * @brief 物理仿真
         * @param time 时间差 */
        void PhysicsEmulator(FLOAT_ time);

        // 物理信息更新
        void PhysicsInformationUpdate();

        /**
         * @brief 获取世界内的能量
         * @return 能量 */
        FLOAT_ GetWorldEnergy();
    };

}