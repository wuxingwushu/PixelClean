#pragma once
#include "PhysicsParticle.hpp"
#include "BaseOutline.hpp"
#include "BaseDefine.h"
#include "PhysicsFormwork.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 物理形状 */
    class PhysicsShape : public PhysicsParticle, public BaseOutline
    {
    public:
        double MomentInertia = 1;    // 转动惯量
        double invMomentInertia = 1; // 转动惯量倒数
        double angle = 0;            // 角度
        double angleSpeed = 0;       // 角速度
        glm::dvec2 CentreMass{0};    // 质心
        glm::dvec2 CentreShape{0};   // 几何中心
        double torque = 0;           // 扭矩
        double CollisionR = 1;       // 碰撞半径
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
         * @param pos 位置(世界位置)
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
         * @brief 几何形状靠近这个点
         * @param drop 点的位置 */
        void ApproachDrop(glm::dvec2 drop);

        /*=========BaseGrid=========*/

        /**
         * @brief 射线碰撞检测
         * @param Pos 射线经过的点
         * @param Angle 射线角度
         * @return 返回碰撞点（精准）*/
        CollisionInfoD RayCollide(glm::dvec2 Pos, double Angle);

        /**
         * @brief 线段侦测(int)
         * @param start 起始位置
         * @param end 结束位置
         * @return 碰撞结果信息 */
        CollisionInfoI PsBresenhamDetection(glm::ivec2 start, glm::ivec2 end);

        /**
         * @brief 线段侦测(double)
         * @param start 起始位置
         * @param end 结束位置
         * @return 碰撞结果信息(准确位置) */
        CollisionInfoD PsBresenhamDetection(glm::dvec2 start, glm::dvec2 end);

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
        virtual PhysicsObjectEnum PFGetType() { return PhysicsObjectEnum::shape; }
        /**
         * @brief 获取碰撞半径
         * @return 半径 */
        virtual double PFGetCollisionR() { return CollisionR; }
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
