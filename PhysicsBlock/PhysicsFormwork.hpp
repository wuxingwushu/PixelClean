#pragma once
#include <climits>
#include "BaseStruct.hpp"

namespace PhysicsBlock
{

    class PhysicsAssembly;

    /**
     * @brief 动态物体的类模板 */
    class PhysicsFormwork
    {
    public:
        PhysicsAssembly *assembly = nullptr;
        PhysicsObjectEnum type = PhysicsObjectEnum::Null;
        unsigned int mGridIndex = UINT_MAX;

        PhysicsFormwork(){};
        ~PhysicsFormwork(){};


        /**
         * @brief 物理外部施加能量对速度的改变
         * @param time 时间差
         * @param Ga 重力加速度 */
        virtual void PhysicsSpeed(FLOAT_ time, Vec2_ Ga) {};

        /**
         * @brief 物理速度对物体的改变
         * @param time 时间差
         * @param Ga 重力加速度 */
        virtual void PhysicsPos(FLOAT_ time, Vec2_ Ga) {};


        /**
         * @brief 获取对象类型
         * @return 类型 */
        PhysicsObjectEnum PFGetType(){ return type; }
        /**
         * @brief 获取碰撞半径
         * @return 半径 */
        virtual FLOAT_ PFGetCollisionR(){ return 0; }
        /**
         * @brief 获取位置
         * @return 位置 */
        virtual Vec2_ PFGetPos(){ return {0,0}; }
        /**
         * @brief 获取旋转角度（弧度）
         * @return 角度；无旋转的派生（如粒子）返回 0
         * @note 用于移动组件做朝向平滑插值，避免依赖 RTTI cast */
        virtual FLOAT_ PFGetAngle(){ return 0; }
        /**
         * @brief 设置旋转角度（弧度）
         * @param angle 目标角度
         * @note 无旋转的派生（如粒子）为 no-op；供移动组件平滑设置朝向 */
        virtual void PFSetAngle(FLOAT_ angle){ (void)angle; }
        /**
         * @brief 获取质量倒数
         * @return 质量倒数 */
        virtual FLOAT_ PFGetInvMass(){ return 0; }
        /**
         * @brief 获取质量
         * @return 质量 */
        virtual FLOAT_ PFGetMass() { return 0; }
        /**
         * @brief 获取转动惯量倒数
         * @return 转动惯量倒数 */
        virtual FLOAT_ PFGetInvI(){ return 0; }
        /**
         * @brief 速度
         * @return 速度 */
        virtual Vec2_& PFSpeed() = 0;
        /**
         * @brief 角速度
         * @return 角速度 */
        virtual FLOAT_& PFAngleSpeed() = 0;

        /**
         * @brief 施加质心力（仅影响平移）
         * @param Force 要施加的力向量
         * @note 带默认空实现，地图/运动学等不响应力的派生可保持空实现 */
        virtual void AddForce(Vec2_ Force) { (void)Force; }

        /**
         * @brief 施加带受力点的力（同时影响平移和旋转）
         * @param Pos 受力点（世界坐标）
         * @param Force 力向量
         * @note 带默认空实现；平动体（无旋转）可退化为质心力 */
        virtual void AddForce(Vec2_ Pos, Vec2_ Force) { (void)Pos; AddForce(Force); }

        /**
         * @brief 施加质心冲量（瞬时改变速度：Δv = invMass * impulse）
         * @param impulse 冲量向量
         * @note 用于击退、跳跃、爆炸击飞等"瞬时"动作；静态/运动学体（invMass==0）静默忽略 */
        virtual void ApplyImpulse(const Vec2_& impulse) { (void)impulse; }

        /**
         * @brief 施加带受力点的冲量（同时产生角速度）
         * @param impulse 冲量向量
         * @param worldPoint 受力点（世界坐标）
         * @note 用于带自旋的击退；平动体退化为质心冲量 */
        virtual void ApplyImpulse(const Vec2_& impulse, const Vec2_& worldPoint) {
            (void)worldPoint; ApplyImpulse(impulse);
        }

        /**
         * @brief 施加纯角冲量（瞬时改变角速度：Δω = invI * torqueImpulse）
         * @param torqueImpulse 角冲量大小
         * @note 平动体无旋转，默认 no-op */
        virtual void ApplyTorqueImpulse(FLOAT_ torqueImpulse) { (void)torqueImpulse; }
    };

}