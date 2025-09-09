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
     * @brief 绳结 */
    struct CordKnot
    {
        PhysicsFormwork *ptr; // 对象指针（空指针为固定位置）
        Vec2_ relativePos;    // 相对位置\固定位置
    };

    /**
     * @brief 绳子类型 */
    enum CordType
    {
        cord,   // 绳子
        spring, // 弹簧
        lever,  // 杠杆
        rubber, // 橡皮筋
    };

    class BaseJunction SerializationInherit_
    {
#if PhysicsBlock_Serialization
    public:
        virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            data["Length"] = Length;
        }
        virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            Length = data["Length"];
        }
#endif
    public:
        FLOAT_ Length;  // 绳子长度
        FLOAT_ bias{0}; // 距离差
        Vec2_ Normal;   // 力的方向
        Vec2_ P;        //

        CordType Type; // 绳子类型

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

        // 预处理
        virtual void PreStep(FLOAT_ inv_dt) = 0;
        // 迭代出结果
        virtual void ApplyImpulse() = 0;
        // 获取绳子的A端
        virtual Vec2_ GetA() = 0;
        // 获取绳子的B端
        virtual Vec2_ GetB() = 0;
    };

    class PhysicsJunctionSS : public BaseJunction
    {
#if PhysicsBlock_Serialization
    public:
        virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            BaseJunction::JsonSerialization(data);
            SerializationVec2(data, mArm1);
            SerializationVec2(data, mArm2);
        }
        virtual void JsonContrarySerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            BaseJunction::JsonContrarySerialization(data);
            ContrarySerializationVec2(data, mArm1);
            ContrarySerializationVec2(data, mArm2);
        }
#endif
    private:
        PhysicsAngle *mParticle1; // 形状
        Vec2_ mArm1;
        Vec2_ mR1;
        PhysicsAngle *mParticle2; // 形状
        Vec2_ mArm2;
        Vec2_ mR2;

    public:
        PhysicsJunctionSS(PhysicsAngle *Particle1, Vec2_ arm1, PhysicsAngle *Particle2, Vec2_ arm2, CordType type);
        ~PhysicsJunctionSS();

        // 预处理
        virtual void PreStep(FLOAT_ inv_dt);
        // 迭代出结果
        virtual void ApplyImpulse();

        virtual Vec2_ GetA() { return mParticle1->pos + vec2angle(mArm1, mParticle1->angle); };

        virtual Vec2_ GetB() { return mParticle2->pos + vec2angle(mArm2, mParticle2->angle); };
    };

    class PhysicsJunctionS : public BaseJunction
    {
#if PhysicsBlock_Serialization
    public:
        virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            BaseJunction::JsonSerialization(data);
            SerializationVec2(data, Arm);
        }
        virtual void JsonContrarySerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            BaseJunction::JsonContrarySerialization(data);
            ContrarySerializationVec2(data, Arm);
        }
#endif
    private:
        Vec2_ mRegularDrop;      // 固定点
        PhysicsAngle *mParticle; // 形状
        Vec2_ Arm;
        Vec2_ R;

    public:
        PhysicsJunctionS(PhysicsAngle *Particle, Vec2_ arm, Vec2_ RegularDrop, CordType type);
        ~PhysicsJunctionS();

        // 预处理
        virtual void PreStep(FLOAT_ inv_dt);
        // 迭代出结果
        virtual void ApplyImpulse();

        virtual Vec2_ GetA() { return mParticle->pos + vec2angle(Arm, mParticle->angle); };

        virtual Vec2_ GetB() { return mRegularDrop; };
    };

    class PhysicsJunctionP : public BaseJunction
    {
#if PhysicsBlock_Serialization
    public:
        virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            BaseJunction::JsonSerialization(data);
            SerializationVec2(data, mRegularDrop);
        }
        virtual void JsonContrarySerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
        {
            BaseJunction::JsonContrarySerialization(data);
            ContrarySerializationVec2(data, mRegularDrop);
        }
#endif
    private:
        Vec2_ mRegularDrop;         // 固定点
        PhysicsParticle *mParticle; // 粒子
    public:
        PhysicsJunctionP(PhysicsParticle *Particle, Vec2_ RegularDrop, CordType type);
        ~PhysicsJunctionP();

        // 预处理
        virtual void PreStep(FLOAT_ inv_dt);
        // 迭代出结果
        virtual void ApplyImpulse();

        virtual Vec2_ GetA() { return mParticle->pos; };

        virtual Vec2_ GetB() { return mRegularDrop; };
    };

    class PhysicsJunctionPP : public BaseJunction
    {
    private:
        PhysicsParticle *mParticle1; // 粒子1
        PhysicsParticle *mParticle2; // 粒子2
    public:
        PhysicsJunctionPP(PhysicsParticle *Particle1, PhysicsParticle *Particle2, CordType type);
        ~PhysicsJunctionPP();

        // 预处理
        virtual void PreStep(FLOAT_ inv_dt);
        // 迭代出结果
        virtual void ApplyImpulse();

        virtual Vec2_ GetA() { return mParticle1->pos; };

        virtual Vec2_ GetB() { return mParticle2->pos; };
    };

}
