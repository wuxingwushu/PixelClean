#pragma once
#include "PhysicsParticle.hpp"
#include "BaseCalculate.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 物理角度类
     * @details 继承自 PhysicsParticle，添加了角度、角速度、转动惯量和扭矩等属性
     * 用于处理具有旋转能力的物理对象
     * 
     * @note 包含以下主要属性：
     * - 转动惯量 (MomentInertia)：物体抵抗旋转的能力
     * - 转动惯量倒数 (invMomentInertia)：转动惯量的倒数，用于优化计算
     * - 角度 (angle)：物体的旋转角度
     * - 角速度 (angleSpeed)：物体的旋转速度
     * - 扭矩 (torque)：作用在物体上的旋转力
     * - 碰撞半径 (radius)：用于碰撞检测的半径
     */
    class PhysicsAngle : public PhysicsParticle
    {
    public:
        /**
         * @brief 转动惯量
         * @details 物体抵抗旋转的能力，与质量分布和形状有关
         */
        FLOAT_ MomentInertia = 1;    
        
        /**
         * @brief 转动惯量倒数
         * @details 转动惯量的倒数，用于优化计算，避免除法操作
         */
        FLOAT_ invMomentInertia = 1; 
        
        /**
         * @brief 角度
         * @details 物体的旋转角度，以弧度为单位
         */
        FLOAT_ angle = 0;            
        
        /**
         * @brief 角速度
         * @details 物体的旋转速度，以弧度/秒为单位
         */
        FLOAT_ angleSpeed = 0;       
        
        /**
         * @brief 扭矩
         * @details 作用在物体上的旋转力，导致物体旋转
         */
        FLOAT_ torque = 0;           
        
        /**
         * @brief 碰撞半径
         * @details 用于碰撞检测的半径，定义了物体的碰撞边界
         */
        FLOAT_ radius;               

#if PhysicsBlock_Serialization
        /**
         * @brief 从 JSON 数据构造物理角度对象
         * @param data JSON 数据
         */
        PhysicsAngle(const nlohmann::json_abi_v3_12_0::basic_json<> &data);

        /**
         * @brief 将物理角度对象序列化为 JSON
         * @param data 输出的 JSON 数据
         */
        virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data);
        
        /**
         * @brief 从 JSON 数据反序列化物理角度对象
         * @param data JSON 数据
         */
        virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data);
#endif

        /**
         * @brief 构造函数
         * @param Pos 位置
         * @param Mass 质量
         * @param Friction 摩擦因数
         * @param Radius 碰撞半径
         */
        PhysicsAngle(Vec2_ Pos, FLOAT_ Mass, FLOAT_ Friction, FLOAT_ Radius);
        
        /**
         * @brief 构造函数（无半径参数）
         * @param Pos 位置
         * @param Mass 质量
         * @param Friction 摩擦因数，默认为0.2
         */
        PhysicsAngle(Vec2_ Pos, FLOAT_ Mass, FLOAT_ Friction = 0.2);
        
        /**
         * @brief 构造函数（仅位置参数）
         * @param Pos 位置
         */
        PhysicsAngle(Vec2_ Pos);
        
        /**
         * @brief 析构函数
         */
        ~PhysicsAngle();

        /**
         * @brief 受力（移动，旋转）
         * @param Pos 受力位置（世界位置）
         * @param Force 力向量
         * @details 同时影响物体的移动和旋转
         */
        void AddForce(Vec2_ Pos, Vec2_ Force);

        /**
         * @brief 受力（移动）
         * @param Force 力向量
         * @details 仅影响物体的移动，不影响旋转
         */
        void AddForce(Vec2_ Force);

        /**
         * @brief 受力（旋转）
         * @param ArmForce 力臂
         * @param Force 力大小
         * @details 仅影响物体的旋转，不影响移动
         */
        void AddTorque(FLOAT_ ArmForce, FLOAT_ Force);

        /**
         * @brief 根据外力和时间更新速度和角速度
         * @param time 时间差（秒）
         * @param Ga 重力加速度向量
         */
        void inline PhysicsSpeed(FLOAT_ time, Vec2_ Ga);

        /**
         * @brief 根据速度和时间更新位置和角度
         * @param time 时间差（秒）
         * @param Ga 重力加速度向量
         */
        void inline PhysicsPos(FLOAT_ time, Vec2_ Ga);

        /*=========PhysicsFormwork=========*/
        /**
         * @brief 获取转动惯量倒数
         * @return 转动惯量倒数
         * @note 实现了 PhysicsFormwork 中的虚函数
         */
        virtual FLOAT_ PFGetInvI() { return invMomentInertia; }
        
        /**
         * @brief 获取角速度引用
         * @return 角速度的引用
         * @note 实现了 PhysicsFormwork 中的虚函数
         */
        virtual FLOAT_ &PFAngleSpeed() { return angleSpeed; };
        
        /**
         * @brief 获取碰撞半径
         * @return 碰撞半径
         * @note 实现了 PhysicsFormwork 中的虚函数
         */
        virtual FLOAT_ PFGetCollisionR() { return radius; }
    };

}
