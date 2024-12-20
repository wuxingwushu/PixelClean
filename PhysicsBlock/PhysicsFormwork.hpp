#pragma once
#include "glm/glm.hpp"
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
         * @brief 获取对象类型
         * @return 类型 */
        virtual PhysicsObjectEnum PFGetType(){ return PhysicsObjectEnum::Null; }
        /**
         * @brief 获取碰撞半径
         * @return 半径 */
        virtual double PFGetCollisionR(){ return 0; }
        /**
         * @brief 获取位置
         * @return 位置 */
        virtual glm::dvec2 PFGetPos(){ return {0,0}; }

        /**
         * @brief 物理仿真
         * @param time 时间差
         * @param Ga 重力加速度 */
        virtual void PhysicsEmulator(double time, glm::dvec2 Ga) {};
    };

}