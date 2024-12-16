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

    struct CordKnot
    {
        glm::dvec2 *pos;        // 对象位置
        double *angle;          // 对象角度
        glm::dvec2 relativePos; // 相对位置
    };

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
    private:
        CordKnot knot1;
        CordKnot knot2;
        CordType type;

    public:
        PhysicsJunction(CordKnot Knot1, CordKnot Knot2, CordType Type) : knot1(Knot1), knot2(Knot2), type(Type) {};
        ~PhysicsJunction() {};
    };

}