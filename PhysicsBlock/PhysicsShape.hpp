#pragma once
#include "PhysicsParticle.hpp"
#include "BaseOutline.hpp"
#include "BaseDefine.h"

namespace PhysicsBlock
{

    /**
     * @brief 物理形状 */
    class PhysicsShape : public PhysicsParticle, BaseOutline
    {
    public:
        double MomentInertia = 1; // 转动惯量
        double angle = 0;         // 角度
        double angleSpeed = 0;    // 角速度
        glm::dvec2 CentreMass;    // 质心
        glm::dvec2 CentreShape;   // 几何中心
        double torque = 0;        // 扭矩
    public:
        /**
         * @brief 构造函数
         * @param Pos 位置
         * @param Size 网格大小 */
        PhysicsShape(glm::dvec2 Pos, glm::ivec2 Size);
        ~PhysicsShape();

        GridBlock &Gridat(unsigned int i) { return at(i); }

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
         * @brief 更新外轮廓 */
        void UpdateOutline() { BaseOutline::UpdateOutline(); }

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
