#pragma once
#include "PhysicsParticle.hpp"
#include "PhysicsShape.hpp"
#include "BaseCalculate.hpp"

/**
 * 连接约束希望不单单是两个连接段互动，和场景也是可以互动的
 * 那绳子和橡皮筋就应该分成多段连接，每段长度小于一个格子的单位长度。
 * 每个链接点都可以做碰撞检测，这样就可以和场景互动了
 */

namespace PhysicsBlock
{

    /**
     * @brief 绳结
     * @details 表示绳子或链条中的一个连接点
     * @param ptr 对象指针（空指针为固定位置）
     * @param relativePos 相对位置（相对对象质心）/固定位置（世界坐标） */
    struct CordKnot
    {
        PhysicsFormwork *ptr; // 对象指针（空指针为固定位置）
        Vec2_ relativePos;    // 相对位置（相对对象质心）/固定位置（世界坐标）
    };

    /**
     * @brief 绳子类型
     * @details 用于区分不同物理特性的连接约束 */
    enum CordType
    {
        cord,   // 绳子（不可伸长）
        spring, // 弹簧（可压缩和拉伸）
        lever,  // 杠杆（硬性连接）
        rubber, // 橡皮筋（可拉伸有弹性）
    };

    /** @brief 连接对象类型 */
    enum CordObjectType
    {
        JunctionAA, // 两个角度物体连接
        JunctionA,  // 单个角度物体连接到固定点
        JunctionP,  // 单个粒子连接到固定点
        JunctionPP, // 两个粒子连接
    };

    /**
     * @brief 连接约束基类
     * @details 提供了连接两个物理对象的约束功能，支持绳子、弹簧、杠杆、橡皮筋等类型 */
    class BaseJunction SerializationInherit_
    {
#if PhysicsBlock_Serialization
    public:
        /** @brief JSON序列化 */
        virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            data["Length"] = Length;
            data["Type"] = Type;
            data["k"] = k;
        }
        /** @brief JSON反序列化 */
        virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            Length = data["Length"];
            Type = data["Type"];
            k = data["k"];
        }
#endif
    public:
        FLOAT_ Length;  // 绳子长度
        FLOAT_ bias{0}; // 距离差（用于位置纠正）
        FLOAT_ k{0};    // 冲量因子
        Vec2_ Normal;   // 力的方向（从B指向A）
        Vec2_ P;        // 冲量累积

        CordType Type; // 绳子类型

        /** @brief 根据绳子类型处理偏置值
         *  @details cord和rubber不允许伸长，spring允许压缩和拉伸但系数不同 */
        void biasType()
        {
            switch (Type)
            {
            case cord:
                if (bias > 0)
                {
                    bias = 0;
                }
                break;
            case rubber:
                if (bias > 0)
                {
                    bias = 0;
                }
                else
                {
                    bias /= 100;
                }
                break;

            case spring:
                bias /= 50;
                break;

            default:
                break;
            }
        }

        /** @brief 根据绳子类型获取弹性系数
         *  @return 弹性系数 */
        FLOAT_ ElasticityType()
        {
            switch (Type)
            {
            case cord:
                return 1;
                break;
            case rubber:
                return 0.005;
                break;
            case spring:
                return 0.005;
                break;

            default:
                break;
            }
            return 1;
        }

        /** @brief 预处理阶段
         *  @param inv_dt 时间步长的倒数 */
        virtual void PreStep(FLOAT_ inv_dt) = 0;
        /** @brief 迭代应用冲量阶段 */
        virtual void ApplyImpulse() = 0;
        /** @brief 获取绳子的A端（连接点1）的世界坐标
         *  @return A端位置向量 */
        virtual Vec2_ GetA() = 0;
        /** @brief 获取绳子的B端（连接点2）的世界坐标
         *  @return B端位置向量 */
        virtual Vec2_ GetB() = 0;
        /** @brief 获取连接对象类型
         *  @return 连接对象类型枚举 */
        virtual CordObjectType ObjectType() = 0;
    };

    /**
     * @brief 两个角度物体间的连接约束
     * @details 用于连接两个PhysicsAngle类型物体（如刚体） */
    class PhysicsJunctionSS : public BaseJunction
    {
#if PhysicsBlock_Serialization
    public:
        /** @brief JSON序列化 */
        virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            BaseJunction::JsonSerialization(data);
            SerializationVec2(data, mArm1);
            SerializationVec2(data, mArm2);
        }
        /** @brief JSON反序列化 */
        virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            BaseJunction::JsonContrarySerialization(data);
            ContrarySerializationVec2(data, mArm1);
            ContrarySerializationVec2(data, mArm2);
        }

        /** @brief 从JSON构造 */
        PhysicsJunctionSS(const nlohmann::json_abi_v3_12_0::basic_json<> &data){
            JsonContrarySerialization(data);
        }
#endif
    public:
        PhysicsAngle *mParticle1; // 形状物体1
        Vec2_ mArm1;              // 物体1上的连接臂（局部坐标）
        Vec2_ mR1;                // 物体1上的连接臂（世界坐标）
        PhysicsAngle *mParticle2; // 形状物体2
        Vec2_ mArm2;              // 物体2上的连接臂（局部坐标）
        Vec2_ mR2;                // 物体2上的连接臂（世界坐标）

    public:
        /** @brief 构造两个角度物体间的连接约束
         *  @param Particle1 形状物体1
         *  @param arm1 物体1上的连接臂（局部坐标）
         *  @param Particle2 形状物体2
         *  @param arm2 物体2上的连接臂（局部坐标）
         *  @param type 连接类型（绳子/弹簧/杠杆/橡皮筋） */
        PhysicsJunctionSS(PhysicsAngle *Particle1, Vec2_ arm1, PhysicsAngle *Particle2, Vec2_ arm2, CordType type);
        /** @brief 析构函数 */
        ~PhysicsJunctionSS();

        /** @brief 预处理阶段，计算约束的normal和bias */
        virtual void PreStep(FLOAT_ inv_dt);
        /** @brief 迭代应用冲量阶段 */
        virtual void ApplyImpulse();

        /** @brief 获取绳子的A端世界坐标
         *  @return 物体1连接点的世界坐标 */
        virtual Vec2_ GetA() { return mParticle1->pos + vec2angle(mArm1, mParticle1->angle); };

        /** @brief 获取绳子的B端世界坐标
         *  @return 物体2连接点的世界坐标 */
        virtual Vec2_ GetB() { return mParticle2->pos + vec2angle(mArm2, mParticle2->angle); };

        /** @brief 获取连接对象类型
         *  @return JunctionAA */
        virtual CordObjectType ObjectType() { return JunctionAA; };
    };

    /**
     * @brief 角度物体到固定点的连接约束
     * @details 一端连接PhysicsAngle物体，另一端固定在空间中某点 */
    class PhysicsJunctionS : public BaseJunction
    {
#if PhysicsBlock_Serialization
    public:
        /** @brief JSON序列化 */
        virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            BaseJunction::JsonSerialization(data);
            SerializationVec2(data, Arm);
            SerializationVec2(data, mRegularDrop);
        }
        /** @brief JSON反序列化 */
        virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            BaseJunction::JsonContrarySerialization(data);
            ContrarySerializationVec2(data, Arm);
            ContrarySerializationVec2(data, mRegularDrop);
        }

        /** @brief 从JSON构造 */
        PhysicsJunctionS(const nlohmann::json_abi_v3_12_0::basic_json<> &data){
            JsonContrarySerialization(data);
        }
#endif
    public:
        Vec2_ mRegularDrop;      // 固定点位置
        PhysicsAngle *mParticle; // 形状物体
        Vec2_ Arm;               // 连接臂（局部坐标）
        Vec2_ R;                 // 连接臂（世界坐标）

    public:
        /** @brief 构造角度物体到固定点的连接约束
         *  @param Particle 形状物体
         *  @param arm 连接臂（局部坐标）
         *  @param RegularDrop 固定点位置
         *  @param type 连接类型 */
        PhysicsJunctionS(PhysicsAngle *Particle, Vec2_ arm, Vec2_ RegularDrop, CordType type);
        /** @brief 析构函数 */
        ~PhysicsJunctionS();

        /** @brief 预处理阶段 */
        virtual void PreStep(FLOAT_ inv_dt);
        /** @brief 迭代应用冲量阶段 */
        virtual void ApplyImpulse();

        /** @brief 获取绳子的A端世界坐标
         *  @return 物体连接点的世界坐标 */
        virtual Vec2_ GetA() { return mParticle->pos + vec2angle(Arm, mParticle->angle); };

        /** @brief 获取绳子的B端世界坐标
         *  @return 固定点位置 */
        virtual Vec2_ GetB() { return mRegularDrop; };

        /** @brief 获取连接对象类型
         *  @return JunctionA */
        virtual CordObjectType ObjectType() { return JunctionA; };
    };

    /**
     * @brief 粒子到固定点的连接约束
     * @details 一端连接PhysicsParticle粒子，另一端固定在空间中某点 */
    class PhysicsJunctionP : public BaseJunction
    {
#if PhysicsBlock_Serialization
    public:
        /** @brief JSON序列化 */
        virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            BaseJunction::JsonSerialization(data);
            SerializationVec2(data, mRegularDrop);
        }
        /** @brief JSON反序列化 */
        virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            BaseJunction::JsonContrarySerialization(data);
            ContrarySerializationVec2(data, mRegularDrop);
        }

        /** @brief 从JSON构造 */
        PhysicsJunctionP(const nlohmann::json_abi_v3_12_0::basic_json<> &data){
            JsonContrarySerialization(data);
        }
#endif
    public:
        Vec2_ mRegularDrop;         // 固定点位置
        PhysicsParticle *mParticle; // 粒子对象
    public:
        /** @brief 构造粒子到固定点的连接约束
         *  @param Particle 粒子对象
         *  @param RegularDrop 固定点位置
         *  @param type 连接类型 */
        PhysicsJunctionP(PhysicsParticle *Particle, Vec2_ RegularDrop, CordType type);
        /** @brief 析构函数 */
        ~PhysicsJunctionP();

        /** @brief 预处理阶段 */
        virtual void PreStep(FLOAT_ inv_dt);
        /** @brief 迭代应用冲量阶段 */
        virtual void ApplyImpulse();

        /** @brief 获取绳子的A端世界坐标
         *  @return 粒子位置 */
        virtual Vec2_ GetA() { return mParticle->pos; };

        /** @brief 获取绳子的B端世界坐标
         *  @return 固定点位置 */
        virtual Vec2_ GetB() { return mRegularDrop; };

        /** @brief 获取连接对象类型
         *  @return JunctionP */
        virtual CordObjectType ObjectType() { return JunctionP; };
    };

    /**
     * @brief 两个粒子间的连接约束
     * @details 直接连接两个PhysicsParticle粒子 */
    class PhysicsJunctionPP : public BaseJunction
    {
#if PhysicsBlock_Serialization
    public:
        /** @brief 从JSON构造 */
        PhysicsJunctionPP(const nlohmann::json_abi_v3_12_0::basic_json<> &data){
            JsonContrarySerialization(data);
        }
#endif
    public:
        PhysicsParticle *mParticle1; // 粒子1
        PhysicsParticle *mParticle2; // 粒子2
    public:
        /** @brief 构造两个粒子间的连接约束
         *  @param Particle1 粒子1
         *  @param Particle2 粒子2
         *  @param type 连接类型 */
        PhysicsJunctionPP(PhysicsParticle *Particle1, PhysicsParticle *Particle2, CordType type);
        /** @brief 析构函数 */
        ~PhysicsJunctionPP();

        /** @brief 预处理阶段 */
        virtual void PreStep(FLOAT_ inv_dt);
        /** @brief 迭代应用冲量阶段 */
        virtual void ApplyImpulse();

        /** @brief 获取绳子的A端世界坐标
         *  @return 粒子1位置 */
        virtual Vec2_ GetA() { return mParticle1->pos; };

        /** @brief 获取绳子的B端世界坐标
         *  @return 粒子2位置 */
        virtual Vec2_ GetB() { return mParticle2->pos; };

        /** @brief 获取连接对象类型
         *  @return JunctionPP */
        virtual CordObjectType ObjectType() { return JunctionPP; };
    };

}