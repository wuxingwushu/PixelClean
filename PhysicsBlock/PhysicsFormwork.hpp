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
         * @brief 获取质量倒数
         * @return 质量倒数 */
        virtual double PFGetInvMass(){ return 0; }
        /**
         * @brief 获取转动惯量倒数
         * @return 转动惯量倒数 */
        virtual double PFGetInvI(){ return 0; }
        /**
         * @brief 速度
         * @return 位置 */
        virtual glm::dvec2& PFSpeed() = 0;
        /**
         * @brief 角速度
         * @return 质量倒数 */
        virtual double& PFAngleSpeed() = 0;
    };

}