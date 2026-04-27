#pragma once
#include "BaseDefine.h"
#include "PhysicsFormwork.hpp"
#include "BaseSerialization.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 物理粒子类
     * @details 表示一个具有质量、位置、速度和受力的物理粒子
     * 继承自 PhysicsFormwork 并实现了序列化接口
     * 
     * @note 包含以下主要属性：
     * - 位置 (pos)：粒子在世界坐标系中的位置
     * - 速度 (speed)：粒子的运动速度
     * - 受力 (force)：作用在粒子上的力
     * - 质量 (mass)：粒子的质量
     * - 质量倒数 (invMass)：质量的倒数，用于优化计算
     * - 静止次数 (StaticNum)：粒子连续静止的次数
     * - 上一时刻位置 (OldPos)：粒子上一时刻的位置
     */
    class PhysicsParticle : public PhysicsFormwork SerializationInherit
    {
#if PhysicsBlock_Serialization
    public:
        /**
         * @brief 序列化虚函数
         */
        SerializationVirtualFunction;
        
        /**
         * @brief 从 JSON 数据构造粒子
         * @param data JSON 数据
         */
        PhysicsParticle(const nlohmann::json_abi_v3_12_0::basic_json<> &data);
#endif
    public:
        /**
         * @brief 静止次数
         * @details 用于判断粒子是否处于静止状态的计数器
         */
        unsigned char StaticNum = 0; 

        /**
         * @brief 上一时刻的位置
         * @note 旧位置不可以在碰撞体内，用于碰撞检测和响应
         */
        Vec2_ OldPos{0};              
        
        /**
         * @brief 是否可以更新位置
         * @details 控制是否允许更新粒子的位置
         */
        bool OldPosUpDataBool = true; 
        
        /**
         * @brief 位置
         * @warning 在形状当中是代表质心在世界坐标的位置
         */
        Vec2_ pos{0};
        
        /**
         * @brief 速度
         * @details 粒子的线性速度向量
         */
        Vec2_ speed{0};        
        
        /**
         * @brief 受力
         * @details 作用在粒子上的合外力向量
         */
        Vec2_ force{0};        
        
        /**
         * @brief 质量
         * @details 粒子的质量，必须大于0
         */
        FLOAT_ mass = 0;       
        
        /**
         * @brief 质量倒数
         * @details 质量的倒数，用于优化计算，避免除法操作
         */
        FLOAT_ invMass = 0;    
        
        /**
         * @brief 摩擦因数
         * @details 粒子与其他物体接触时的摩擦系数，默认为0.2
         */
        FLOAT_ friction = 0.2; 

    public:
        /**
         * @brief 构造函数
         * @param Pos 粒子的初始位置
         * @param Mass 粒子的质量
         * @param Friction 摩擦因数，默认为0.2
         */
        PhysicsParticle(Vec2_ Pos, FLOAT_ Mass, FLOAT_ Friction = 0.2);
        
        /**
         * @brief 构造函数（无质量参数）
         * @param Pos 粒子的初始位置
         * @note 质量默认为0，此时粒子将不受力的影响
         */
        PhysicsParticle(Vec2_ Pos);
        
        /**
         * @brief 析构函数
         */
        ~PhysicsParticle();

        /**
         * @brief 添加一个受力
         * @param Force 要添加的力向量
         * @note 力会累加到当前的受力上
         */
        virtual void AddForce(Vec2_ Force);

        /*=========PhysicsFormwork=========*/

        /**
         * @brief 根据外力和时间更新速度
         * @param time 时间差（秒）
         * @param Ga 重力加速度向量
         * @note 实现了 PhysicsFormwork 中的虚函数
         */
        virtual void PhysicsSpeed(FLOAT_ time, Vec2_ Ga);

        /**
         * @brief 根据速度和时间更新位置
         * @param time 时间差（秒）
         * @param Ga 重力加速度向量
         * @note 实现了 PhysicsFormwork 中的虚函数
         */
        virtual void PhysicsPos(FLOAT_ time, Vec2_ Ga);

        /**
         * @brief 获取对象类型
         * @return 物理对象类型，返回 PhysicsObjectEnum::particle
         * @note 实现了 PhysicsFormwork 中的虚函数
         */
        virtual PhysicsObjectEnum PFGetType() { return PhysicsObjectEnum::particle; }
        
        /**
         * @brief 获取位置
         * @return 粒子的当前位置
         * @note 实现了 PhysicsFormwork 中的虚函数
         */
        virtual Vec2_ PFGetPos() { return pos; }
        
        /**
         * @brief 获取质量倒数
         * @return 粒子质量的倒数
         * @note 实现了 PhysicsFormwork 中的虚函数
         */
        virtual FLOAT_ PFGetInvMass() { return invMass; }
        
        /**
         * @brief 获取质量
         * @return 粒子的质量
         * @note 实现了 PhysicsFormwork 中的虚函数
         */
        virtual FLOAT_ PFGetMass() { return mass; }
        
        /**
         * @brief 获取转动惯量倒数
         * @return 0，因为粒子不存在转动惯量
         * @warning 粒子不存在转动惯量，调用此函数会触发断言
         * @note 实现了 PhysicsFormwork 中的虚函数
         */
        virtual FLOAT_ PFGetInvI()
        {
            assert(0 && "[Error]: 粒子不存在转动惯量!");
            return 0;
        }
        
        /**
         * @brief 获取速度引用
         * @return 粒子的速度向量引用
         * @note 实现了 PhysicsFormwork 中的虚函数
         */
        virtual Vec2_ &PFSpeed() { return speed; };
        
        /**
         * @brief 获取角速度引用
         * @return 返回质量倒数的引用，因为粒子不存在角速度
         * @warning 粒子不存在角速度，调用此函数会触发断言
         * @note 实现了 PhysicsFormwork 中的虚函数
         */
        virtual FLOAT_ &PFAngleSpeed()
        {
            assert(0 && "[Error]: 粒子不存在角速度!");
            return invMass;
        };
    };

}
