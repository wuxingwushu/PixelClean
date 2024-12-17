#pragma once
#include "PhysicsParticle.hpp"
#include "BaseOutline.hpp"
#include "BaseDefine.h"

namespace PhysicsBlock
{

    /**
     * @brief 物理形状 */
    class PhysicsShape : public PhysicsParticle, public BaseOutline
    {
    public:
        double MomentInertia = 1; // 转动惯量
        double angle = 0;         // 角度
        double angleSpeed = 0;    // 角速度
        glm::dvec2 CentreMass{0};    // 质心
        glm::dvec2 CentreShape{0};   // 几何中心
        double torque = 0;        // 扭矩

        double CollisionR; // 碰撞半径
    public:
        /**
         * @brief 构造函数
         * @param Pos 位置
         * @param Size 网格大小 */
        PhysicsShape(glm::dvec2 Pos, glm::ivec2 Size);
        ~PhysicsShape();

        GridBlock &Gridat(unsigned int i) { return at(i); }

        /**
         * @brief 检测点是否点击到网格
         * @param pos 位置
         * @return 是否点击，网格位置 */
        CollisionInfoI DropCollision(glm::dvec2 pos);

        /**
         * @brief 受力（移动，旋转）
         * @param Pos 受力位置（世界位置）
         * @param Force 力矩 */
        void AddForce(glm::dvec2 Pos, glm::dvec2 Force);

        /**
         * @brief 受力（移动）
         * @param Force 力矩 */
        void AddForce(glm::dvec2 Force) { PhysicsParticle::AddForce(Force); };

        /**
         * @brief 受力（旋转）
         * @param ArmForce 力臂
         * @param Force 力大小 */
        void AddTorque(double ArmForce, double Force);

        /**
         * @brief 信息更新
         * @note 更新： 总重量， 几何中心， 质心， 转动惯量 */
        void UpdateInfo();

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
