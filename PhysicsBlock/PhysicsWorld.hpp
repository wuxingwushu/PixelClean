#pragma once
#include <vector>
#include "MapFormwork.hpp"     // 地图样板
#include "PhysicsShape.hpp"    // 有形状物体
#include "PhysicsParticle.hpp" // 物理粒子

namespace PhysicsBlock
{

    /**
     * @brief 物理世界
     * @note 重力加速度， 网格风 */
    class PhysicsWorld
    {
    private:
        glm::dvec2 GravityAcceleration;      // 重力加速度
        const bool WindBool;                 // 是否开启网格风
        glm::dvec2 Wind{0};                  // 风
        glm::dvec2 *GridWind = nullptr;      // 网格风
        glm::uvec2 GridWindSize{0};          // 网格风大小
        MapFormwork *wMapFormwork = nullptr; // 地图对象

        std::vector<PhysicsParticle*> PhysicsParticleS;
        std::vector<PhysicsShape*> PhysicsShapeS;
    public:
        /**
         * @brief 构建函数
         * @param GravityAcceleration 重力加速度
         * @param Wind 风是否开启 */
        PhysicsWorld(glm::dvec2 GravityAcceleration, const bool Wind);
        ~PhysicsWorld();

        void AddObject(PhysicsShape* Object){
            PhysicsShapeS.push_back(Object);
        }
        void AddObject(PhysicsParticle* Object){
            PhysicsParticleS.push_back(Object);
        }

        /**
         * @brief 设置地图
         * @param MapFormwork_ 地图指针 */
        void SetMapFormwork(MapFormwork *MapFormwork_);

        /**
         * @brief 物理仿真
         * @param time 时间差 */
        void PhysicsEmulator(double time);
    };

}