#pragma once
#include "BaseArbiter.hpp"
#include "MapFormwork.hpp"
#include "PhysicsParticle.hpp"
#include "PhysicsShape.hpp"

namespace PhysicsBlock
{
    
    /**
     * @brief 动态形状，动态形状，碰撞解析 */
    class PhysicsArbiterSS : public BaseArbiter
    {
        PhysicsShape *object1;
        PhysicsShape *object2;
    public:
        PhysicsArbiterSS(PhysicsShape *Object1, PhysicsShape *Object2);
        ~PhysicsArbiterSS(){};

        // 更新碰撞信息
        virtual void Update(Contact *NewContacts, int numNewContacts);
        // 预处理
        virtual void PreStep(double inv_dt);
        // 迭代出结果
        virtual void ApplyImpulse();
    };

    /**
     * @brief 动态形状，粒子形状，碰撞解析 */
    class PhysicsArbiterSP : public BaseArbiter
    {
        // object1  动态形状
        // object2  粒子形状
        PhysicsShape *object1;
        PhysicsParticle *object2;
    public:
        PhysicsArbiterSP(PhysicsShape *Object1, PhysicsParticle *Object2);
        ~PhysicsArbiterSP(){};

        // 更新碰撞信息
        virtual void Update(Contact *NewContacts, int numNewContacts);
        // 预处理
        virtual void PreStep(double inv_dt);
        // 迭代出结果
        virtual void ApplyImpulse();
    };

    /**
     * @brief 动态形状，地形，碰撞解析 */
    class PhysicsArbiterS : public BaseArbiter
    {
        // object1  动态形状
        PhysicsShape *object1;
        MapFormwork *object2;
    public:
        PhysicsArbiterS(PhysicsShape *Object1, MapFormwork *Object2);
        ~PhysicsArbiterS(){};

        // 更新碰撞信息
        virtual void Update(Contact *NewContacts, int numNewContacts);
        // 预处理
        virtual void PreStep(double inv_dt);
        // 迭代出结果
        virtual void ApplyImpulse();
    };

    /**
     * @brief 粒子形状，地形，碰撞解析 */
    class PhysicsArbiterP : public BaseArbiter
    {
        // object1  粒子形状
        PhysicsParticle *object1;
        MapFormwork *object2;
    public:
        PhysicsArbiterP(PhysicsParticle *Object1, MapFormwork *Object2);
        ~PhysicsArbiterP(){};

        // 更新碰撞信息
        virtual void Update(Contact *NewContacts, int numNewContacts);
        // 预处理
        virtual void PreStep(double inv_dt);
        // 迭代出结果
        virtual void ApplyImpulse();
    };
    
}
