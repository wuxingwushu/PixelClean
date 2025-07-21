#pragma once
#include "PhysicsFormwork.hpp"
#include "PhysicsParticle.hpp"

namespace PhysicsBlock
{

    class PhysicsCircle : public PhysicsParticle
    {
    public:
        double radius; // 碰撞半径
        double MomentInertia = 1;    // 转动惯量
        double invMomentInertia = 1; // 转动惯量倒数
        double angle = 0;            // 角度
        double angleSpeed = 0;       // 角速度
        double torque = 0;           // 扭矩
    public:
        PhysicsCircle(glm::dvec2 Pos, double Radius, double Mass);
        ~PhysicsCircle();


        /*=========PhysicsFormwork=========*/

        /**
         * @brief 物理外部施加能量对速度的改变
         * @param time 时间差
         * @param Ga 重力加速度 */
        virtual void PhysicsSpeed(double time, glm::dvec2 Ga);

        /**
         * @brief 物理速度对物体的改变
         * @param time 时间差
         * @param Ga 重力加速度 */
        virtual void PhysicsPos(double time, glm::dvec2 Ga);

        /**
         * @brief 获取对象类型
         * @return 类型 */
        virtual PhysicsObjectEnum PFGetType() { return PhysicsObjectEnum::circle; }
        /**
         * @brief 获取碰撞半径
         * @return 半径 */
        virtual double PFGetCollisionR() { return radius; }
        /**
         * @brief 获取位置
         * @return 位置 */
        virtual glm::dvec2 PFGetPos() { return pos; }
        /**
         * @brief 获取质量倒数
         * @return 质量倒数 */
        virtual double PFGetInvMass(){ return invMass; }
        /**
         * @brief 获取转动惯量倒数
         * @return 转动惯量倒数 */
        virtual double PFGetInvI(){ return invMomentInertia; }
        /**
         * @brief 速度
         * @return 位置 */
        virtual glm::dvec2& PFSpeed() { return speed; };
        /**
         * @brief 角速度
         * @return 质量倒数 */
        virtual double& PFAngleSpeed() { return angleSpeed; };
    };
    
}
