#pragma once
#include <glm/glm.hpp>
#include "PhysicsFormwork.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 物理粒子
     * @note 位置，速度，受力，质量 */
    class PhysicsParticle : public PhysicsFormwork
    {
    public:
        glm::dvec2 OldPos{0}; // 上一时刻的位置
        /**
         * @brief 位置
         * @warning 在形状当中是指质心的位置 */
        glm::dvec2 pos{0};
        glm::dvec2 speed{0}; // 速度
        glm::dvec2 force{0}; // 受力
        double mass = 0;     // 质量
        double invMass = 0;  // 质量倒数

    public:
        PhysicsParticle(glm::dvec2 Pos, double Mass);
        PhysicsParticle(glm::dvec2 Pos);
        ~PhysicsParticle();

        /**
         * @brief 添加 一个受力
         * @param Force 力矩 */
        virtual void AddForce(glm::dvec2 Force);

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
        virtual PhysicsObjectEnum PFGetType() { return PhysicsObjectEnum::particle; }
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
        virtual double PFGetInvI(){ 
            assert(0 && "[Error]: 粒子不存在转动惯量!");
            return 0;
        }
        /**
         * @brief 速度
         * @return 位置 */
        virtual glm::dvec2& PFSpeed() { return speed; };
        /**
         * @brief 角速度
         * @return 质量倒数 */
        virtual double& PFAngleSpeed() {
            assert(0 && "[Error]: 粒子不存在角速度!");
            return invMass;
        };
    };

}
