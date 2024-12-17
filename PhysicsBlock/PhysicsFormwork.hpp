#pragma once
#include "glm/glm.hpp"

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
         * @brief 获取碰撞半径
         * @return 半径 */
        virtual double PFGetCollisionR(){ return 0; }
        /**
         * @brief 获取几何中心位置
         * @return 几何中心位置 */
        virtual glm::dvec2 PFGetCentre(){ return {0}; }
    };

}