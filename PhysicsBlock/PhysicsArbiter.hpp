#pragma once
#include "BaseArbiter.hpp"

namespace PhysicsBlock
{
    
    /**
     * @brief 动态形状，动态形状，碰撞解析 */
    class PhysicsArbiterSS : public BaseArbiter
    {
    public:
        PhysicsArbiterSS(PhysicsFormwork *Object1, PhysicsFormwork *Object2);
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
    public:
        PhysicsArbiterSP(PhysicsFormwork *Object1, PhysicsFormwork *Object2);
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
    public:
        PhysicsArbiterS(PhysicsFormwork *Object1, PhysicsFormwork *Object2);
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
    public:
        PhysicsArbiterP(PhysicsFormwork *Object1, PhysicsFormwork *Object2);
        ~PhysicsArbiterP(){};

        // 更新碰撞信息
        virtual void Update(Contact *NewContacts, int numNewContacts);
        // 预处理
        virtual void PreStep(double inv_dt);
        // 迭代出结果
        virtual void ApplyImpulse();
    };
    
}
