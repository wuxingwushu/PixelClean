#pragma once
#include "BaseArbiter.hpp"
#include "MapFormwork.hpp"
#include "PhysicsAngle.hpp"
#include "PhysicsParticle.hpp"

namespace PhysicsBlock
{

    /**
     * @brief   动态形状与动态形状的碰撞裁决器
     * @details 处理两个 PhysicsAngle 对象之间的碰撞响应
     *          继承自 BaseArbiter，实现了完整的碰撞处理流程
     */
    class PhysicsBaseArbiterAA : public BaseArbiter
    {
    public:
        PhysicsAngle *object1;  ///< 第一个动态形状对象
        PhysicsAngle *object2;  ///< 第二个动态形状对象

        /**
         * @brief   构造函数
         * @param   Object1 第一个动态形状对象
         * @param   Object2 第二个动态形状对象
         */
        PhysicsBaseArbiterAA(PhysicsAngle *Object1, PhysicsAngle *Object2) 
            : BaseArbiter(Object1, Object2), object1(Object1), object2(Object2) {};
        
        /** @brief 析构函数 */
        ~PhysicsBaseArbiterAA() {};

        /**
         * @brief   更新碰撞信息
         * @param   NewContacts 新的碰撞点数组
         * @param   numNewContacts 新碰撞点数量
         * @details 将新碰撞信息复制到内部，并保留之前的冲量累积值
         */
        virtual void Update(Contact *NewContacts, int numNewContacts);
        
        /**
         * @brief   预处理（无时间步长版本）
         * @details 计算碰撞点相对于两个物体质心的位置向量
         */
        virtual void PreStep();
        
        /**
         * @brief   预处理（带时间步长版本）
         * @param   inv_dt 时间步长的倒数
         * @details 计算有效质量、位置修正值，并应用累积冲量
         */
        virtual void PreStep(FLOAT_ inv_dt);
        
        /**
         * @brief   应用冲量
         * @details 根据碰撞信息计算并应用法向冲量和切向冲量（摩擦力）
         */
        virtual void ApplyImpulse();
    };

    /**
     * @brief   动态形状与动态点的碰撞裁决器
     * @details 处理 PhysicsAngle 对象与 PhysicsParticle 对象之间的碰撞响应
     *          点对象没有旋转，因此不计算旋转相关的冲量
     */
    class PhysicsBaseArbiterAD : public BaseArbiter
    {
    public:
        PhysicsAngle *object1;    ///< 动态形状对象
        PhysicsParticle *object2; ///< 动态点对象

        /**
         * @brief   构造函数
         * @param   Object1 动态形状对象
         * @param   Object2 动态点对象
         */
        PhysicsBaseArbiterAD(PhysicsAngle *Object1, PhysicsParticle *Object2) 
            : BaseArbiter(Object1, Object2), object1(Object1), object2(Object2) {};
        
        /** @brief 析构函数 */
        ~PhysicsBaseArbiterAD() {};

        /**
         * @brief   更新碰撞信息
         * @param   NewContacts 新的碰撞点数组
         * @param   numNewContacts 新碰撞点数量
         */
        virtual void Update(Contact *NewContacts, int numNewContacts);
        
        /** @brief 预处理（无时间步长版本） */
        virtual void PreStep();
        
        /**
         * @brief   预处理（带时间步长版本）
         * @param   inv_dt 时间步长的倒数
         */
        virtual void PreStep(FLOAT_ inv_dt);
        
        /** @brief 应用冲量 */
        virtual void ApplyImpulse();
    };


    /**
     * @brief   动态形状与地图的碰撞裁决器
     * @details 处理 PhysicsAngle 对象与 MapFormwork 对象之间的碰撞响应
     *          地图是静态的，因此只更新动态形状的速度
     */
    class PhysicsBaseArbiterA : public BaseArbiter
    {
    public:
        PhysicsAngle *object1;  ///< 动态形状对象
        MapFormwork *object2;   ///< 地图对象（静态）

        /**
         * @brief   构造函数
         * @param   Object1 动态形状对象
         * @param   Object2 地图对象
         */
        PhysicsBaseArbiterA(PhysicsAngle *Object1, MapFormwork *Object2) 
            : BaseArbiter(Object1, Object2), object1(Object1), object2(Object2) {};
        
        /** @brief 析构函数 */
        ~PhysicsBaseArbiterA() {};

        /**
         * @brief   更新碰撞信息
         * @param   NewContacts 新的碰撞点数组
         * @param   numNewContacts 新碰撞点数量
         */
        virtual void Update(Contact *NewContacts, int numNewContacts);
        
        /** @brief 预处理（无时间步长版本） */
        virtual void PreStep();
        
        /**
         * @brief   预处理（带时间步长版本）
         * @param   inv_dt 时间步长的倒数
         */
        virtual void PreStep(FLOAT_ inv_dt);
        
        /** @brief 应用冲量 */
        virtual void ApplyImpulse();
    };

    /**
     * @brief   动态点与地图的碰撞裁决器
     * @details 处理 PhysicsParticle 对象与 MapFormwork 对象之间的碰撞响应
     *          点对象没有旋转，地图是静态的
     */
    class PhysicsBaseArbiterD : public BaseArbiter
    {
    public:
        PhysicsParticle *object1; ///< 动态点对象
        MapFormwork *object2;     ///< 地图对象（静态）

        /**
         * @brief   构造函数
         * @param   Object1 动态点对象
         * @param   Object2 地图对象
         */
        PhysicsBaseArbiterD(PhysicsParticle *Object1, MapFormwork *Object2) 
            : BaseArbiter(Object1, Object2), object1(Object1), object2(Object2) {};
        
        /** @brief 析构函数 */
        ~PhysicsBaseArbiterD() {};

        /**
         * @brief   更新碰撞信息
         * @param   NewContacts 新的碰撞点数组
         * @param   numNewContacts 新碰撞点数量
         */
        virtual void Update(Contact *NewContacts, int numNewContacts);
        
        /** @brief 预处理（无时间步长版本） */
        virtual void PreStep();
        
        /**
         * @brief   预处理（带时间步长版本）
         * @param   inv_dt 时间步长的倒数
         */
        virtual void PreStep(FLOAT_ inv_dt);
        
        /** @brief 应用冲量 */
        virtual void ApplyImpulse();
    };

}
