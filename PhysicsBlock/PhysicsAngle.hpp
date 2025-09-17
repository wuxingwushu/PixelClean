#pragma once
#include "PhysicsParticle.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 基础角度信息样本类（包含 PhysicsParticle）
     * @note 角度，角速度，转动惯量，转动惯量倒数，扭矩 */
    class PhysicsAngle : public PhysicsParticle
    {
    public:
        FLOAT_ MomentInertia = 1;    // 转动惯量
        FLOAT_ invMomentInertia = 1; // 转动惯量倒数
        FLOAT_ angle = 0;            // 角度
        FLOAT_ angleSpeed = 0;       // 角速度
        FLOAT_ torque = 0;           // 扭矩
        FLOAT_ radius;               // 碰撞半径

#if PhysicsBlock_Serialization
        PhysicsAngle(const nlohmann::json_abi_v3_12_0::basic_json<> &data) : PhysicsParticle(data)
        {
            MomentInertia = data["MomentInertia"];
            if (MomentInertia == FLOAT_MAX) {
                invMomentInertia = 0;
            }else{
                invMomentInertia = FLOAT_(1) / MomentInertia;
            }
            angle = data["angle"];
            angleSpeed = data["angleSpeed"];
            torque = data["torque"];
            radius = data["radius"];
        }

        virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            PhysicsParticle::JsonSerialization(data);
            data["MomentInertia"] = MomentInertia;
            data["angle"] = angle;
            data["angleSpeed"] = angleSpeed;
            data["torque"] = torque;
            data["radius"] = radius;
        }
        virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            PhysicsParticle::JsonContrarySerialization(data);
            MomentInertia = data["MomentInertia"];
            if (MomentInertia == FLOAT_MAX) {
                invMomentInertia = 0;
            }else{
                invMomentInertia = FLOAT_(1) / MomentInertia;
            }
            angle = data["angle"];
            angleSpeed = data["angleSpeed"];
            torque = data["torque"];
            radius = data["radius"];
        }
#endif

        PhysicsAngle(Vec2_ Pos, FLOAT_ Mass, FLOAT_ Friction, FLOAT_ Radius) : PhysicsParticle(Pos, Mass, Friction), radius(Radius) {}
        PhysicsAngle(Vec2_ Pos, FLOAT_ Mass, FLOAT_ Friction = 0.2) : PhysicsParticle(Pos, Mass, Friction) {}
        PhysicsAngle(Vec2_ Pos) : PhysicsParticle(Pos) {}
        ~PhysicsAngle() {}

        /**
         * @brief 受力（移动，旋转）
         * @param Pos 受力位置（世界位置）
         * @param Force 力矩 */
        void AddForce(Vec2_ Pos, Vec2_ Force)
        {
            if (invMass == 0)
            {
                return;
            }
            Pos -= pos;                                                    // 质心指向受力点的力矩
            FLOAT_ Langle = EdgeVecToCosAngleFloat(Pos);                   // 力臂角度
            FLOAT_ disparity = Langle - EdgeVecToCosAngleFloat(Force);     // 力矩角度
            FLOAT_ ForceFLOAT_ = Modulus(Force);                           // 力大小
            FLOAT_ F = ForceFLOAT_ * cos(disparity);                       // 垂直力臂的力大小
            PhysicsParticle::AddForce({F * cos(Langle), F * sin(Langle)}); // 转为世界力矩 // 位置移动受到力矩
            AddTorque(Modulus(Pos), ForceFLOAT_ * -sin(disparity));        // 旋转受到扭矩
        }

        /**
         * @brief 受力（移动）
         * @param Force 力矩 */
        void AddForce(Vec2_ Force) { PhysicsParticle::AddForce(Force); };

        /**
         * @brief 受力（旋转）
         * @param ArmForce 力臂
         * @param Force 力大小 */
        void AddTorque(FLOAT_ ArmForce, FLOAT_ Force)
        {
            torque += ArmForce * Force; // 累加扭矩
        }

        void inline PhysicsSpeed(FLOAT_ time, Vec2_ Ga)
        {
            if (invMass == 0)
                return;

            PhysicsParticle::PhysicsSpeed(time, Ga);
            angleSpeed += time * invMomentInertia * torque;
            torque = 0;
        }

        void inline PhysicsPos(FLOAT_ time, Vec2_ Ga)
        {
            if (invMass == 0)
                return;

            PhysicsParticle::PhysicsPos(time, Ga);
            angle += time * angleSpeed;
        }

        /*=========PhysicsFormwork=========*/
        /**
         * @brief 获取转动惯量倒数
         * @return 转动惯量倒数 */
        virtual FLOAT_ PFGetInvI() { return invMomentInertia; }
        /**
         * @brief 角速度
         * @return 质量倒数 */
        virtual FLOAT_ &PFAngleSpeed() { return angleSpeed; };
        /**
         * @brief 获取碰撞半径
         * @return 半径 */
        virtual FLOAT_ PFGetCollisionR() { return radius; }
    };

}
