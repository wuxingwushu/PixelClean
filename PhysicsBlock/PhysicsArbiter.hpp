#pragma once
#include "PhysicsBaseArbiter.hpp"
#include "PhysicsShape.hpp"
#include "PhysicsCircle.hpp"

namespace PhysicsBlock
{
   
    /**
     * @brief 动态形状，动态形状，碰撞解析 */
    class PhysicsArbiterSS : public PhysicsBaseArbiterAA
    {
    public:
        PhysicsArbiterSS(PhysicsShape *Object1, PhysicsShape *Object2):PhysicsBaseArbiterAA(Object1 > Object2 ? Object1 : Object2, Object1 > Object2 ? Object2 : Object1){};
        ~PhysicsArbiterSS(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };

    /**
     * @brief 动态形状，粒子形状，碰撞解析 */
    class PhysicsArbiterSP : public PhysicsBaseArbiterAD
    {
    public:
        PhysicsArbiterSP(PhysicsShape *Object1, PhysicsParticle *Object2):PhysicsBaseArbiterAD(Object1, Object2){};
        ~PhysicsArbiterSP(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };

    /**
     * @brief 动态形状，地形，碰撞解析 */
    class PhysicsArbiterS : public PhysicsBaseArbiterA
    {
    public:
        PhysicsArbiterS(PhysicsShape *Object1, MapFormwork *Object2):PhysicsBaseArbiterA(Object1, Object2){};
        ~PhysicsArbiterS(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };

    /**
     * @brief 粒子形状，地形，碰撞解析 */
    class PhysicsArbiterP : public PhysicsBaseArbiterD
    {
    public:
        PhysicsArbiterP(PhysicsParticle *Object1, MapFormwork *Object2):PhysicsBaseArbiterD(Object1, Object2){};
        ~PhysicsArbiterP(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };


    /**
     * @brief 圆，地形，碰撞解析 */
    class PhysicsArbiterC : public PhysicsBaseArbiterA
    {
    public:
        PhysicsArbiterC(PhysicsCircle *Object1, MapFormwork *Object2):PhysicsBaseArbiterA(Object1, Object2){};
        ~PhysicsArbiterC(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };

    /**
     * @brief 圆，地形，碰撞解析 */
    class PhysicsArbiterCS : public PhysicsBaseArbiterAA
    {
    public:
        PhysicsArbiterCS(PhysicsCircle *Object1, PhysicsShape *Object2):PhysicsBaseArbiterAA(Object1, Object2){};
        ~PhysicsArbiterCS(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };

    /**
     * @brief 圆，地形，碰撞解析 */
    class PhysicsArbiterCP : public PhysicsBaseArbiterAD
    {
    public:
        PhysicsArbiterCP(PhysicsCircle *Object1, PhysicsParticle *Object2):PhysicsBaseArbiterAD(Object1, Object2){};
        ~PhysicsArbiterCP(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };

    /**
     * @brief 圆，地形，碰撞解析 */
    class PhysicsArbiterCC : public PhysicsBaseArbiterAA
    {
    public:
        PhysicsArbiterCC(PhysicsCircle *Object1, PhysicsCircle *Object2):PhysicsBaseArbiterAA(Object1 > Object2 ? Object1 : Object2, Object1 > Object2 ? Object2 : Object1){};
        ~PhysicsArbiterCC(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };
    
}
