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
    public:
        glm::dvec2 GravityAcceleration;      // 重力加速度
        const bool WindBool;                 // 是否开启网格风
        glm::dvec2 Wind{0};                  // 风
        glm::dvec2 *GridWind = nullptr;      // 网格风
        glm::uvec2 GridWindSize{0};          // 网格风大小
        MapFormwork *wMapFormwork = nullptr; // 地图对象

        std::vector<PhysicsFormwork *> PhysicsFormworkS;

        /*************重力**************/
        /**
         * @brief 两个形状物理碰撞处理
         * @param a 形状A(这个是被压的)
         * @param b 形状B
         * @note 被压的：重力方向在下端的那个物体 */
        void PhysicsProcess(PhysicsShape *a, PhysicsShape *b);
        void PhysicsProcess(PhysicsParticle *a, PhysicsShape *b);
        void PhysicsProcess(PhysicsShape *a, PhysicsParticle *b);

        /*************动能**************/
        /**
         * @brief 动能守恒定律
         * @param a 物体A
         * @param b 物体B
         * @warning 理想碰撞，力都转换为速度，没有转换为角速度(可以理解为球体相撞) */
        void EnergyConservation(PhysicsParticle* a, PhysicsParticle* b);
        /**
         * @brief 形状碰撞动能守恒尝试
         * @param a 被撞物体A
         * @param b 物体B
         * @param CollisionDrop 碰撞点
         * @param Vertical 法向量角度(碰撞边的垂直法向量， 向内)
         * @warning 碰撞点在两质心线段上的，（动能 和 角动能 才守恒） */
        void EnergyConservation(PhysicsShape* a, PhysicsShape* b, glm::dvec2 CollisionDrop, double Vertical);
        /**
         * @brief 形状碰撞动能守恒尝试
         * @param a 被撞物体A
         * @param CollisionDrop 碰撞点
         * @param Vertical 法向量角度(碰撞边的垂直法向量， 向内)
         * @warning 碰撞点在两质心线段上的，（动能 和 角动能 才守恒） */
        void EnergyConservation(PhysicsShape* a, glm::dvec2 CollisionDrop, double Vertical);

        /*************位置约束**************/
        // 和地图(时间处理（位置，角度碰）， 位置约束)
        void PositionRestrain(PhysicsShape* a, double time);
        void PositionRestrain(PhysicsParticle* a, double time);
        // 相互位置约束
        void PositionRestrain(PhysicsParticle* a, PhysicsShape* b);
        void PositionRestrain(PhysicsShape* a, PhysicsShape* b);
    public:
        /**
         * @brief 构建函数
         * @param GravityAcceleration 重力加速度
         * @param Wind 风是否开启 */
        PhysicsWorld(glm::dvec2 GravityAcceleration, const bool Wind);
        ~PhysicsWorld();

        void AddObject(PhysicsFormwork *Object)
        {
            PhysicsFormworkS.push_back(Object);
        }

        /**
         * @brief 获取位置上的对象
         * @param pos 位置
         * @return 对象指针
         * @retval nullptr 没有对象 */
        PhysicsFormwork* Get(glm::dvec2 pos);

        /**
         * @brief 设置地图
         * @param MapFormwork_ 地图指针 */
        void SetMapFormwork(MapFormwork *MapFormwork_);

        /**
         * @brief 物理仿真
         * @param time 时间差 */
        void PhysicsEmulator(double time);

        /**
         * @brief 获取世界内的能量
         * @return 能量 */
        double GetWorldEnergy();

        std::vector<PhysicsFormwork*>* GetPhysicsFormworkS() {
            return &PhysicsFormworkS;
        }


    };

}