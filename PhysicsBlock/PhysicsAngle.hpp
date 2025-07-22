#pragma once
#include "PhysicsParticle.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 基础角度信息样本类（包含 PhysicsParticle）
     * @note 角度，角速度，转动惯量，转动惯量倒数，扭矩 */
    class PhysicsAngle : public PhysicsParticle
    {
    public:
        double MomentInertia = 1;    // 转动惯量
        double invMomentInertia = 1; // 转动惯量倒数
        double angle = 0;            // 角度
        double angleSpeed = 0;       // 角速度
        double torque = 0;           // 扭矩
        
        PhysicsAngle(glm::dvec2 Pos, double Mass):PhysicsParticle(Pos, Mass){}
        PhysicsAngle(glm::dvec2 Pos):PhysicsParticle(Pos){}
        ~PhysicsAngle(){}

        /*=========PhysicsFormwork=========*/
        /**
         * @brief 获取转动惯量倒数
         * @return 转动惯量倒数 */
        virtual double PFGetInvI(){ return invMomentInertia; }
        /**
         * @brief 角速度
         * @return 质量倒数 */
        virtual double& PFAngleSpeed() { return angleSpeed; };
    };
    
}
