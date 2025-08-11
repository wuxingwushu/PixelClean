#pragma once
#include "PhysicsFormwork.hpp"
#include "PhysicsAngle.hpp"

namespace PhysicsBlock
{

    class PhysicsLine : public PhysicsAngle
    {
    public:
        FLOAT_ radius; // 半径
    public:
        PhysicsLine(Vec2_ begin_, Vec2_ end_, FLOAT_ mass_, FLOAT_ friction_ = 0.2);
        ~PhysicsLine(){};

        /**
         * @brief 获取对象类型
         * @return 类型 */
        virtual PhysicsObjectEnum PFGetType() { return PhysicsObjectEnum::line; }
        /**
         * @brief 获取碰撞半径
         * @return 半径 */
        virtual FLOAT_ PFGetCollisionR() { return radius; }
    };

}
