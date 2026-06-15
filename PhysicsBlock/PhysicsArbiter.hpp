#pragma once
#include "PhysicsBaseArbiter.hpp"
#include "PhysicsShape.hpp"
#include "PhysicsCircle.hpp"
#include "PhysicsLine.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 动态形状，动态形状，碰撞解析 */
    class PhysicsArbiterSS : public PhysicsBaseArbiterAA
    {
    public:
        PhysicsArbiterSS(PhysicsShape *Object1, PhysicsShape *Object2):PhysicsBaseArbiterAA(Object1 > Object2 ? Object1 : Object2, Object1 > Object2 ? Object2 : Object1) { mArbiterType = PhysicsArbiterType::ArbiterSS; }
        ~PhysicsArbiterSS(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };

    /**
     * @brief 动态形状，粒子形状，碰撞解析 */
    class PhysicsArbiterSP : public PhysicsBaseArbiterAD
    {
    public:
        PhysicsArbiterSP(PhysicsShape *Object1, PhysicsParticle *Object2):PhysicsBaseArbiterAD(Object1, Object2) { mArbiterType = PhysicsArbiterType::ArbiterSP; }
        ~PhysicsArbiterSP(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };

    /**
     * @brief 动态形状，地形，碰撞解析 */
    class PhysicsArbiterS : public PhysicsBaseArbiterA
    {
    public:
        PhysicsArbiterS(PhysicsShape *Object1, MapFormwork *Object2):PhysicsBaseArbiterA(Object1, Object2) { mArbiterType = PhysicsArbiterType::ArbiterS; }
        ~PhysicsArbiterS(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };

    /**
     * @brief 粒子形状，地形，碰撞解析 */
    class PhysicsArbiterP : public PhysicsBaseArbiterD
    {
    public:
        PhysicsArbiterP(PhysicsParticle *Object1, MapFormwork *Object2):PhysicsBaseArbiterD(Object1, Object2) { mArbiterType = PhysicsArbiterType::ArbiterP; }
        ~PhysicsArbiterP(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };


    /**
     * @brief 圆，地形，碰撞解析 */
    class PhysicsArbiterC : public PhysicsBaseArbiterA
    {
    public:
        PhysicsArbiterC(PhysicsCircle *Object1, MapFormwork *Object2):PhysicsBaseArbiterA(Object1, Object2) { mArbiterType = PhysicsArbiterType::ArbiterC; }
        ~PhysicsArbiterC(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };

    /**
     * @brief 圆，网格形状，碰撞解析 */
    class PhysicsArbiterCS : public PhysicsBaseArbiterAA
    {
    public:
        PhysicsArbiterCS(PhysicsCircle *Object1, PhysicsShape *Object2):PhysicsBaseArbiterAA(Object1, Object2) { mArbiterType = PhysicsArbiterType::ArbiterCS; }
        ~PhysicsArbiterCS(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };

    /**
     * @brief 圆，点，碰撞解析 */
    class PhysicsArbiterCP : public PhysicsBaseArbiterAD
    {
    public:
        PhysicsArbiterCP(PhysicsCircle *Object1, PhysicsParticle *Object2):PhysicsBaseArbiterAD(Object1, Object2) { mArbiterType = PhysicsArbiterType::ArbiterCP; }
        ~PhysicsArbiterCP(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };

    /**
     * @brief 圆，圆，碰撞解析 */
    class PhysicsArbiterCC : public PhysicsBaseArbiterAA
    {
    public:
        PhysicsArbiterCC(PhysicsCircle *Object1, PhysicsCircle *Object2):PhysicsBaseArbiterAA(Object1 > Object2 ? Object1 : Object2, Object1 > Object2 ? Object2 : Object1) { mArbiterType = PhysicsArbiterType::ArbiterCC; }
        ~PhysicsArbiterCC(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };

    /**
     * @brief 线，圆，碰撞解析 */
    class PhysicsArbiterLC : public PhysicsBaseArbiterAA
    {
    public:
        PhysicsArbiterLC(PhysicsLine *Object1, PhysicsCircle *Object2):PhysicsBaseArbiterAA(Object1, Object2) { mArbiterType = PhysicsArbiterType::ArbiterLC; }
        ~PhysicsArbiterLC(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };

    /**
     * @brief 线，点，碰撞解析 */
    class PhysicsArbiterLP : public PhysicsBaseArbiterAD
    {
    public:
        PhysicsArbiterLP(PhysicsLine *Object1, PhysicsParticle *Object2):PhysicsBaseArbiterAD(Object1, Object2) { mArbiterType = PhysicsArbiterType::ArbiterLP; }
        ~PhysicsArbiterLP(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };

    /**
     * @brief 线，网格形状，碰撞解析 */
    class PhysicsArbiterLS : public PhysicsBaseArbiterAA
    {
    public:
        PhysicsArbiterLS(PhysicsLine *Object1, PhysicsShape *Object2):PhysicsBaseArbiterAA(Object1, Object2) { mArbiterType = PhysicsArbiterType::ArbiterLS; }
        ~PhysicsArbiterLS(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };

    /**
     * @brief 线，地形，碰撞解析 */
    class PhysicsArbiterL : public PhysicsBaseArbiterA
    {
    public:
        PhysicsArbiterL(PhysicsLine *Object1, MapFormwork *Object2):PhysicsBaseArbiterA(Object1, Object2) { mArbiterType = PhysicsArbiterType::ArbiterL; }
        ~PhysicsArbiterL(){};

        // 计算俩物体的碰撞
        virtual void ComputeCollide();
    };
    
}
