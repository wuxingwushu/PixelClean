#pragma once
#include "PhysicsFormwork.hpp"
#include "PhysicsAngle.hpp"

namespace PhysicsBlock
{

    class PhysicsCircle : public PhysicsAngle
    {
    public:
        FLOAT_ radius; // 碰撞半径
    public:
        PhysicsCircle(Vec2_ Pos, FLOAT_ Radius, FLOAT_ Mass, FLOAT_ Friction = 0.2);
        ~PhysicsCircle();


        /*=========PhysicsFormwork=========*/

        /**
         * @brief 物理外部施加能量对速度的改变
         * @param time 时间差
         * @param Ga 重力加速度 */
        virtual void PhysicsSpeed(FLOAT_ time, Vec2_ Ga);

        /**
         * @brief 物理速度对物体的改变
         * @param time 时间差
         * @param Ga 重力加速度 */
        virtual void PhysicsPos(FLOAT_ time, Vec2_ Ga);

        /**
         * @brief 获取对象类型
         * @return 类型 */
        virtual PhysicsObjectEnum PFGetType() { return PhysicsObjectEnum::circle; }
        /**
         * @brief 获取碰撞半径
         * @return 半径 */
        virtual FLOAT_ PFGetCollisionR() { return radius; }
    };
    
}
