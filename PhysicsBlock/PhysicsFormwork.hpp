#pragma once
#include "BaseStruct.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 动态物体的类模板 */
    class PhysicsFormwork
    {
    public:
        PhysicsFormwork(){};
        ~PhysicsFormwork(){};


        /**
         * @brief 物理外部施加能量对速度的改变
         * @param time 时间差
         * @param Ga 重力加速度 */
        virtual void PhysicsSpeed(FLOAT_ time, Vec2_ Ga) {};

        /**
         * @brief 物理速度对物体的改变
         * @param time 时间差
         * @param Ga 重力加速度 */
        virtual void PhysicsPos(FLOAT_ time, Vec2_ Ga) {};


        /**
         * @brief 获取对象类型
         * @return 类型 */
        virtual PhysicsObjectEnum PFGetType(){ return PhysicsObjectEnum::Null; }
        /**
         * @brief 获取碰撞半径
         * @return 半径 */
        virtual FLOAT_ PFGetCollisionR(){ return 0; }
        /**
         * @brief 获取位置
         * @return 位置 */
        virtual Vec2_ PFGetPos(){ return {0,0}; }
        /**
         * @brief 获取质量倒数
         * @return 质量倒数 */
        virtual FLOAT_ PFGetInvMass(){ return 0; }
        /**
         * @brief 获取转动惯量倒数
         * @return 转动惯量倒数 */
        virtual FLOAT_ PFGetInvI(){ return 0; }
        /**
         * @brief 速度
         * @return 位置 */
        virtual Vec2_& PFSpeed() = 0;
        /**
         * @brief 角速度
         * @return 质量倒数 */
        virtual FLOAT_& PFAngleSpeed() = 0;
    };

}