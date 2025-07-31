#pragma once
#include "PhysicsFormwork.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 物理粒子
     * @note 位置，速度，受力，质量，质量倒数，静止次数，上一次刻的位置 */
    class PhysicsParticle : public PhysicsFormwork
    {
    public:
        unsigned char StaticNum = 0; // 静止次数

        Vec2_ OldPos{0}; // 上一时刻的位置（ 旧位置不可以在碰撞体内 ）
        bool OldPosUpDataBool = true; // 是否可以更新位置
        /**
         * @brief 位置
         * @warning 在形状当中是指质心的位置 */
        Vec2_ pos{0};
        Vec2_ speed{0}; // 速度
        Vec2_ force{0}; // 受力
        FLOAT_ mass = 0;     // 质量
        FLOAT_ invMass = 0;  // 质量倒数
        FLOAT_ friction = 0.2;  // 摩擦因数

    public:
        PhysicsParticle(Vec2_ Pos, FLOAT_ Mass, FLOAT_ Friction = 0.2);
        PhysicsParticle(Vec2_ Pos);
        ~PhysicsParticle();

        /**
         * @brief 添加 一个受力
         * @param Force 力矩 */
        virtual void AddForce(Vec2_ Force);

        /*=========PhysicsFormwork=========*/

        /**
         * @brief 物理外部施加能量对速度的改变
         * @param time 时间差
         * @param Ga 重力加速度 */
        virtual void PhysicsSpeed(FLOAT_ time, Vec2_ Ga);

        /**
         * @brief 物理速度对物体的改变
         * @param time 时间差
         * @param Ga 重力加速度 */
        virtual void PhysicsPos(FLOAT_ time, Vec2_ Ga);

        /**
         * @brief 获取对象类型
         * @return 类型 */
        virtual PhysicsObjectEnum PFGetType() { return PhysicsObjectEnum::particle; }
        /**
         * @brief 获取位置
         * @return 位置 */
        virtual Vec2_ PFGetPos() { return pos; }
        /**
         * @brief 获取质量倒数
         * @return 质量倒数 */
        virtual FLOAT_ PFGetInvMass(){ return invMass; }
        /**
         * @brief 获取质量
         * @return 质量 */
        virtual FLOAT_ PFGetMass() { return mass; }
        /**
         * @brief 获取转动惯量倒数
         * @return 转动惯量倒数 */
        virtual FLOAT_ PFGetInvI(){ 
            assert(0 && "[Error]: 粒子不存在转动惯量!");
            return 0;
        }
        /**
         * @brief 速度
         * @return 位置 */
        virtual Vec2_& PFSpeed() { return speed; };
        /**
         * @brief 角速度
         * @return 质量倒数 */
        virtual FLOAT_& PFAngleSpeed() {
            assert(0 && "[Error]: 粒子不存在角速度!");
            return invMass;
        };
    };

}
