#pragma once
#include "PhysicsParticle.hpp"
#include "PhysicsShape.hpp"

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

    /**
     * @brief 物理连接
     * @note 绳子， 弹簧， 杠杆 */
    class PhysicsJunction
    {
    public:
        CordKnot objectA;                   // 被绑定对象A
        CordKnot objectB;                   // 被绑定对象B
        unsigned int KnotSize;  // 结点数B
        PhysicsParticle **PhysicsParticleS; // 绳子每个结点
    private:
        const CordType type;    // 绳子类型
        double Length;          // 绳子每小段的长度
        double coefficient = 1; // 弹簧系数

        /**
         * @brief 解算他两受力
         * @param pos 固定位置
         * @param B 物理粒子 */
        void PhysicsAnalytic(glm::dvec2 pos, PhysicsParticle *B);
        /**
         * @brief 解算他两受力
         * @param A 物理粒子
         * @param B 物理粒子 */
        void PhysicsAnalytic(PhysicsParticle *A, PhysicsParticle *B);
        /**
         * @brief 解算他两受力
         * @param A 物理形状
         * @param Arm 形状质心偏移位置
         * @param B 物理粒子 */
        void PhysicsAnalytic(PhysicsShape *A, glm::dvec2 Arm, PhysicsParticle *B);

    public:
        PhysicsJunction(CordKnot object1, CordKnot object2, const CordType Type);
        ~PhysicsJunction();

        /**
         * @brief 计算绳子每个节点的受力 */
        void BearForceAnalytic();
    };

}