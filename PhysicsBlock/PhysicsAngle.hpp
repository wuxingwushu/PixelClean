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
        FLOAT_ MomentInertia = 1;    // 转动惯量
        FLOAT_ invMomentInertia = 1; // 转动惯量倒数
        FLOAT_ angle = 0;            // 角度
        FLOAT_ angleSpeed = 0;       // 角速度
        FLOAT_ torque = 0;           // 扭矩
        
        PhysicsAngle(Vec2_ Pos, FLOAT_ Mass):PhysicsParticle(Pos, Mass){}
        PhysicsAngle(Vec2_ Pos):PhysicsParticle(Pos){}
        ~PhysicsAngle(){}

        /*=========PhysicsFormwork=========*/
        /**
         * @brief 获取转动惯量倒数
         * @return 转动惯量倒数 */
        virtual FLOAT_ PFGetInvI(){ return invMomentInertia; }
        /**
         * @brief 角速度
         * @return 质量倒数 */
        virtual FLOAT_& PFAngleSpeed() { return angleSpeed; };
    };
    
}
