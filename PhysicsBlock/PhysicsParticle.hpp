#pragma once
#include <glm/glm.hpp>

namespace PhysicsBlock
{

    /**
     * @brief 物理粒子
     * @note 位置，速度，受力，质量 */
    class PhysicsParticle
    {
    public:
        glm::dvec2 pos;   // 位置
        glm::dvec2 speed; // 速度
        glm::dvec2 force; // 受力
        double mass;      // 质量
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
    };

}
