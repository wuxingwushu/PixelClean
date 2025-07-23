#pragma once
#include "BaseArbiter.hpp"
#include "MapFormwork.hpp"
#include "PhysicsAngle.hpp"
#include "PhysicsParticle.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 动态形状，动态形状，碰撞解析 */
    class PhysicsBaseArbiterAA : public BaseArbiter
    {
    public:
        PhysicsAngle *object1;
        PhysicsAngle *object2;

        PhysicsBaseArbiterAA(PhysicsAngle *Object1, PhysicsAngle *Object2) : BaseArbiter(Object1, Object2), object1(Object1), object2(Object2) {};
        ~PhysicsBaseArbiterAA() {};

        // 更新碰撞信息
        virtual void Update(Contact *NewContacts, int numNewContacts);
        // 预处理
        virtual void PreStep(FLOAT_ inv_dt);
        virtual void PreStep();
        // 迭代出结果
        virtual void ApplyImpulse();
    };

    /**
     * @brief 动态形状，动态点，碰撞解析 */
    class PhysicsBaseArbiterAD : public BaseArbiter
    {
    public:
        PhysicsAngle *object1;
        PhysicsParticle *object2;

        PhysicsBaseArbiterAD(PhysicsAngle *Object1, PhysicsParticle *Object2) : BaseArbiter(Object1, Object2), object1(Object1), object2(Object2) {};
        ~PhysicsBaseArbiterAD() {};

        // 更新碰撞信息
        virtual void Update(Contact *NewContacts, int numNewContacts);
        // 预处理
        virtual void PreStep(FLOAT_ inv_dt);
        virtual void PreStep();
        // 迭代出结果
        virtual void ApplyImpulse();
    };


    /**
     * @brief 动态形状，地图，碰撞解析 */
    class PhysicsBaseArbiterA : public BaseArbiter
    {
    public:
        PhysicsAngle *object1;
        MapFormwork *object2;

        PhysicsBaseArbiterA(PhysicsAngle *Object1, MapFormwork *Object2) : BaseArbiter(Object1, Object2), object1(Object1), object2(Object2) {};
        ~PhysicsBaseArbiterA() {};

        // 更新碰撞信息
        virtual void Update(Contact *NewContacts, int numNewContacts);
        // 预处理
        virtual void PreStep(FLOAT_ inv_dt);
        virtual void PreStep();
        // 迭代出结果
        virtual void ApplyImpulse();
    };

    /**
     * @brief 动态点，地图，碰撞解析 */
    class PhysicsBaseArbiterD : public BaseArbiter
    {
    public:
        PhysicsParticle *object1;
        MapFormwork *object2;

        PhysicsBaseArbiterD(PhysicsParticle *Object1, MapFormwork *Object2) : BaseArbiter(Object1, Object2), object1(Object1), object2(Object2) {};
        ~PhysicsBaseArbiterD() {};

        // 更新碰撞信息
        virtual void Update(Contact *NewContacts, int numNewContacts);
        // 预处理
        virtual void PreStep(FLOAT_ inv_dt);
        virtual void PreStep();
        // 迭代出结果
        virtual void ApplyImpulse();
    };

}