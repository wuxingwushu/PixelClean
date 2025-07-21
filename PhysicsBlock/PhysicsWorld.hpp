#pragma once
#include <vector>
#include "MapFormwork.hpp"     // 地图样板
#include "PhysicsShape.hpp"    // 有形状物体
#include "PhysicsParticle.hpp" // 物理粒子
#include "PhysicsArbiter.hpp"  // 物理解析單元
#include "PhysicsJoint.hpp"    // 物理关节
#include "PhysicsJunction.hpp" // 物理连接
#include <map>

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
    /**
     * @brief 物理世界
     * @note 重力加速度， 网格风 */
    class PhysicsWorld
    {
    public:
        glm::dvec2 GravityAcceleration;      // 重力加速度
        const bool WindBool;                 // 是否开启网格风
        glm::dvec2 Wind{0};                  // 风
        glm::dvec2 *GridWind = nullptr;      // 网格风
        glm::uvec2 GridWindSize{0};          // 网格风大小
        MapFormwork *wMapFormwork = nullptr; // 地图对象

        double inv_dt;

        std::vector<PhysicsShape*> PhysicsShapeS;
        std::vector<PhysicsParticle*> PhysicsParticleS;
        std::vector<PhysicsJoint*> PhysicsJointS;
        std::vector<BaseJunction*> BaseJunctionS;
        std::vector<PhysicsCircle*> PhysicsCircleS;

        std::map<ArbiterKey, BaseArbiter*> CollideGroupS;// 碰撞队
        
        void HandleCollideGroup(BaseArbiter* Ba);
        #if MemoryPoolBool
        std::mutex mLockSP;
        std::mutex mLockSS;
        std::mutex mLockS;
        std::mutex mLockP;
        std::mutex mLockC;
        MemoryPool<PhysicsArbiterSP, 10000 * sizeof(PhysicsArbiterSP)> PoolPhysicsArbiterSP;
        MemoryPool<PhysicsArbiterSS, 10000 * sizeof(PhysicsArbiterSS)> PoolPhysicsArbiterSS;
        MemoryPool<PhysicsArbiterS, 10000 * sizeof(PhysicsArbiterS)> PoolPhysicsArbiterS;
        MemoryPool<PhysicsArbiterP, 10000 * sizeof(PhysicsArbiterP)> PoolPhysicsArbiterP;
        MemoryPool<PhysicsArbiterC, 10000 * sizeof(PhysicsArbiterC)> PoolPhysicsArbiterC;
        #endif
        void Arbiter(PhysicsShape* S, PhysicsParticle* P);
        void Arbiter(PhysicsShape* S1, PhysicsShape* S2);
        void Arbiter(PhysicsShape* S, MapFormwork* M);
        void Arbiter(PhysicsParticle* P, MapFormwork* M);
        void Arbiter(PhysicsCircle* P, MapFormwork* M);

        void DeleteArbiter(BaseArbiter* BA);

#if ThreadPoolBool
        std::mutex mLock;
        ThreadPool mThreadPool;
#endif
    public:
        /**
         * @brief 构建函数
         * @param GravityAcceleration 重力加速度
         * @param Wind 风是否开启 */
        PhysicsWorld(glm::dvec2 GravityAcceleration, const bool Wind);
        ~PhysicsWorld();

        void AddObject(PhysicsShape *Object)
        {
            PhysicsShapeS.push_back(Object);
        }

        void AddObject(PhysicsParticle *Object)
        {
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
            PhysicsCircleS.push_back(Object);
        }

        std::vector<PhysicsShape*>& GetPhysicsShape() {
            return PhysicsShapeS;
        }

        std::vector<PhysicsParticle*>& GetPhysicsParticle() {
            return PhysicsParticleS;
        }

        std::vector<PhysicsJoint*>& GetPhysicsJoint() {
            return PhysicsJointS;
        }

        std::vector<BaseJunction*>& GetBaseJunction() {
            return BaseJunctionS;
        }

        std::vector<PhysicsCircle*>& GetPhysicsCircle() {
            return PhysicsCircleS;
        }

        MapFormwork* GetMapFormwork() {
            return wMapFormwork;
        }

        /**
         * @brief 获取位置上的对象
         * @param pos 位置
         * @return 对象指针
         * @retval nullptr 没有对象 */
        PhysicsFormwork *Get(glm::dvec2 pos);

        /**
         * @brief 设置地图
         * @param MapFormwork_ 地图指针 */
        void SetMapFormwork(MapFormwork *MapFormwork_);

        /**
         * @brief 物理仿真
         * @param time 时间差 */
        void PhysicsEmulator(double time);

        // 物理信息更新
        void PhysicsInformationUpdate();

        /**
         * @brief 获取世界内的能量
         * @return 能量 */
        double GetWorldEnergy();

    };

}