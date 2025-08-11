#pragma once
#include "PhysicsAngle.hpp"
#include "BaseOutline.hpp"
#include "BaseDefine.h"
#include "PhysicsFormwork.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 物理形状 */
    class PhysicsShape : public PhysicsAngle, public BaseOutline
    {
    public:
        Vec2_ CentreMass{0};    // 质心
        Vec2_ CentreShape{0};   // 几何中心
        FLOAT_ CollisionR = 1;       // 碰撞半径
    public:
        /**
         * @brief 构造函数
         * @param Pos 位置
         * @param Size 网格大小 */
        PhysicsShape(Vec2_ Pos, glm::ivec2 Size);
        ~PhysicsShape();

        GridBlock &Gridat(unsigned int i) { return at(i); }

        /**
         * @brief 检测点是否点击到网格
         * @param pos 位置(世界位置)
         * @return 是否点击，网格位置 */
        CollisionInfoI DropCollision(Vec2_ pos);

        /**
         * @brief 受力（旋转）
         * @param ArmForce 力臂
         * @param Force 力大小 */
        void AddTorque(FLOAT_ ArmForce, FLOAT_ Force);

        /**
         * @brief 信息更新
         * @note 更新： 总重量， 几何中心， 质心， 转动惯量 */
        void UpdateInfo();

        /**
         * @brief 半径计算
         * @note 更新：半径
         * @warning 必须先计算 Outline */
        void UpdateCollisionR();

        /**
         * @brief 更新全部信息 */
        void UpdateAll();

        /**
         * @brief 几何形状靠近这个点
         * @param drop 点的位置 */
        void ApproachDrop(Vec2_ drop);

        /*=========BaseGrid=========*/

        /**
         * @brief 射线碰撞检测
         * @param Pos 射线经过的点
         * @param Angle 射线角度
         * @return 返回碰撞点（精准）*/
        CollisionInfoD RayCollide(Vec2_ Pos, FLOAT_ Angle);

        /**
         * @brief 线段侦测(int)
         * @param start 起始位置
         * @param end 结束位置
         * @return 碰撞结果信息 */
        CollisionInfoI PsBresenhamDetection(glm::ivec2 start, glm::ivec2 end);

        /**
         * @brief 线段侦测(FLOAT_)
         * @param start 起始位置
         * @param end 结束位置
         * @return 碰撞结果信息(准确位置) */
        CollisionInfoD PsBresenhamDetection(Vec2_ start, Vec2_ end);

        /*=========PhysicsFormwork=========*/

        /**
         * @brief 获取对象类型
         * @return 类型 */
        virtual PhysicsObjectEnum PFGetType() { return PhysicsObjectEnum::shape; }
        /**
         * @brief 获取碰撞半径
         * @return 半径 */
        virtual FLOAT_ PFGetCollisionR() { return CollisionR; }
        /**
         * @brief 获取位置
         * @return 位置 */
        virtual Vec2_ PFGetPos() { return pos; }
        /**
         * @brief 获取质量倒数
         * @return 质量倒数 */
        virtual FLOAT_ PFGetInvMass(){ return invMass; }
        /**
         * @brief 获取转动惯量倒数
         * @return 转动惯量倒数 */
        virtual FLOAT_ PFGetInvI(){ return invMomentInertia; }
        /**
         * @brief 速度
         * @return 位置 */
        virtual Vec2_& PFSpeed() { return speed; };
        /**
         * @brief 角速度
         * @return 质量倒数 */
        virtual FLOAT_& PFAngleSpeed() { return angleSpeed; };
    };

}
