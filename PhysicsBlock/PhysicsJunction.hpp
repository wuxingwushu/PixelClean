#pragma once
#include "PhysicsParticle.hpp"
#include "PhysicsShape.hpp"
#include "BaseCalculate.hpp"

/**
 * 连接约束希望不单单是两个连接段互动，和场景也是可以互动的
 * 那绳子和橡皮筋就应该分成多段连接，每段长度小于一个格子的单位长度。
 * 每个链接点都可以做碰撞检测，这样就可以和场景互动了
 */

namespace PhysicsBlock
{

    /**
     * @brief 绳结 */
    struct CordKnot
    {
        PhysicsFormwork *ptr;   // 对象指针（空指针为固定位置）
        glm::dvec2 relativePos; // 相对位置\固定位置
    };

    /**
     * @brief 绳子类型 */
    enum CordType
    {
        cord,   // 绳子
        spring, // 弹簧
        lever,  // 杠杆
        rubber, // 橡皮筋
    };

#define JISHU 1.0

    class BaseJunction
    {
    public:
        double Length;     // 绳子长度
        double bias{0};    // 距离差
        glm::dvec2 Normal; // 力的方向
        glm::dvec2 P;      //

        // 预处理
        virtual void PreStep(double inv_dt) = 0;
        // 迭代出结果
        virtual void ApplyImpulse() = 0;
        // 获取绳子的A端
        virtual glm::dvec2 GetA() = 0;
        // 获取绳子的B端
        virtual glm::dvec2 GetB() = 0;
    };

    class PhysicsJunctionSS : public BaseJunction
    {
    private:
        PhysicsShape *mParticle1; // 形状
        glm::dvec2 mArm1;
        glm::dvec2 mR1;
        PhysicsShape *mParticle2; // 形状
        glm::dvec2 mArm2;
        glm::dvec2 mR2;


    public:
        PhysicsJunctionSS(PhysicsShape *Particle1, glm::dvec2 arm1, PhysicsShape *Particle2, glm::dvec2 arm2);
        ~PhysicsJunctionSS();

        // 预处理
        virtual void PreStep(double inv_dt);
        // 迭代出结果
        virtual void ApplyImpulse();

        virtual glm::dvec2 GetA() { return mParticle1->pos + vec2angle(mArm1, mParticle1->angle); };

        virtual glm::dvec2 GetB() { return mParticle2->pos + vec2angle(mArm2, mParticle2->angle); };
    };

    class PhysicsJunctionS : public BaseJunction
    {
    private:
        glm::dvec2 mRegularDrop; // 固定点
        PhysicsShape *mParticle; // 形状
        glm::dvec2 Arm;
        glm::dvec2 R;

    public:
        PhysicsJunctionS(PhysicsShape *Particle, glm::dvec2 arm, glm::dvec2 RegularDrop);
        ~PhysicsJunctionS();

        // 预处理
        virtual void PreStep(double inv_dt);
        // 迭代出结果
        virtual void ApplyImpulse();

        virtual glm::dvec2 GetA() { return mParticle->pos + vec2angle(Arm, mParticle->angle); };

        virtual glm::dvec2 GetB() { return mRegularDrop; };
    };

    class PhysicsJunctionP : public BaseJunction
    {
    private:
        glm::dvec2 mRegularDrop;    // 固定点
        PhysicsParticle *mParticle; // 粒子
    public:
        PhysicsJunctionP(PhysicsParticle *Particle, glm::dvec2 RegularDrop);
        ~PhysicsJunctionP();

        // 预处理
        virtual void PreStep(double inv_dt);
        // 迭代出结果
        virtual void ApplyImpulse();

        virtual glm::dvec2 GetA() { return mParticle->pos; };

        virtual glm::dvec2 GetB() { return mRegularDrop; };
    };

    class PhysicsJunctionPP : public BaseJunction
    {
    private:
        PhysicsParticle *mParticle1; // 粒子1
        PhysicsParticle *mParticle2; // 粒子2
    public:
        PhysicsJunctionPP(PhysicsParticle *Particle1, PhysicsParticle *Particle2);
        ~PhysicsJunctionPP();

        // 预处理
        virtual void PreStep(double inv_dt);
        // 迭代出结果
        virtual void ApplyImpulse();

        virtual glm::dvec2 GetA() { return mParticle1->pos; };

        virtual glm::dvec2 GetB() { return mParticle2->pos; };
    };

}
