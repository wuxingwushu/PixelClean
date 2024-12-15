#pragma once
#include "PhysicsParticle.hpp"
#include "PhysicsShape.hpp"

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