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

        /**
         * @brief 位置
         * @warning 在形状当中是指质心的位置 */
        glm::dvec2 pos{0};   
        glm::dvec2 speed{0}; // 速度
        glm::dvec2 force{0}; // 受力
        double mass = 0;      // 质量

    public:
        PhysicsParticle(glm::dvec2 Pos, double Mass);
        PhysicsParticle(glm::dvec2 Pos);
        ~PhysicsParticle();

        /**
         * @brief 添加 一个受力
         * @param Force 力矩 */
        virtual void AddForce(glm::dvec2 Force);

        /**
         * @brief 物理仿真
         * @param time 时间差
         * @param Ga 重力加速度 */
        virtual void PhysicsEmulator(double time, glm::dvec2 Ga);
        
        /**
         * @brief 物理演戏
         * @param time 时间差
         * @param Ga 重力加速度 */
        virtual glm::dvec2 PhysicsPlayact(double time, glm::dvec2 Ga);

        /*=========PhysicsFormwork=========*/

        /**
         * @brief 获取对象类型
         * @return 类型 */
        virtual PhysicsObjectEnum PFGetType(){ return PhysicsObjectEnum::particle; }
        /**
         * @brief 获取位置
         * @return 位置 */
        virtual glm::dvec2 PFGetPos(){ return pos; }
    };

}
