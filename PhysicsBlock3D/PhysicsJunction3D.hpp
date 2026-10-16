#pragma once
#include "PhysicsAngle3D.hpp"
#include "PhysicsParticle3D.hpp"
#include "PhysicsFormwork3D.hpp"

namespace PhysicsBlock3D
{

    /**
     * @brief 连接端点（3D）
     * @details ptr 为空指针时表示该端固定在世界坐标 relativePos 处 */
    struct CordKnot3D
    {
        PhysicsFormwork3D *ptr;
        Vec3_ relativePos;
    };

    /** @brief 连接类型 */
    enum CordType3D
    {
        cord3D,   // 绳子（只能拉不能推）
        spring3D, // 弹簧
        lever3D,  // 杠杆（刚体）
        rubber3D  // 橡皮筋
    };

    /** @brief 连接对象类型 */
    enum CordObjectType3D
    {
        JunctionAA3D,
        JunctionA3D,
        JunctionP3D,
        JunctionPP3D
    };

    /**
     * @brief 连接约束基类（3D）
     * @details 3D 相对 2D 的关键变化：
     *            - 法线 `Normal`、累计冲量 `P` 由 `Vec2_` 变 `Vec3_`；
     *            - 有效质量 k 的转动项由 2D 的 `invI*(r×n)²` 推广为
     *              `(r×n)·I⁻¹·(r×n)`（等价于 `n·((I⁻¹(r×n))×r)`），
     *              且 `I⁻¹` 必须取世界系 `WorldInvInertia()`；
     *            - 角速度由标量变 `Vec3_`，叉积由标量变向量；
     *            - 新增 `L ≈ 0` 保护（2D 两端重合时 `Normal = dp/0` 产生 NaN）。 */
    class BaseJunction3D SerializationInherit3D_
    {
    public:
        FLOAT_ Length;      // 静止长度
        FLOAT_ bias{0};     // 位置修正量
        FLOAT_ k{0};        // 有效质量（2D 注释称"倒数"，实为有效质量本身）
        Vec3_ Normal;       // A→B 归一化方向
        Vec3_ P;            // 累计冲量（2D 声明但从未使用，3D 同样保留以备扩展）
        CordType3D Type;    // 连接类型

#if PhysicsBlock_Serialization3D
    public:
        SerializationVirtualFunction3D;
        BaseJunction3D() {};
        /** @brief 从JSON反序列化构造 */
        BaseJunction3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data) { JsonContrarySerialization(data); };
        virtual ~BaseJunction3D() {};
#else
        BaseJunction3D() {};
        virtual ~BaseJunction3D() {};
#endif

        /** @brief 根据连接类型修正 bias */
        inline void biasType()
        {
            switch (Type)
            {
            case cord3D:
                // 绳子只能拉，不能推
                if (bias > 0)
                {
                    bias = 0;
                }
                break;
            case rubber3D:
                if (bias > 0)
                {
                    bias = 0;
                }
                else
                {
                    bias /= 100;
                }
                break;
            case spring3D:
                bias /= 50;
                break;
            default:
                break;
            }
        }

        /** @brief 根据连接类型返回弹性系数 */
        inline FLOAT_ ElasticityType()
        {
            switch (Type)
            {
            case cord3D:
                return 1;
            case rubber3D:
                return 0.005;
            case spring3D:
                return 0.005;
            default:
                return 1;
            }
        }

        /** @brief 预处理阶段 */
        virtual void PreStep(FLOAT_ inv_dt) = 0;

        /** @brief 应用冲量阶段 */
        virtual void ApplyImpulse() = 0;

        /** @brief 获取连接点 A 的世界坐标 */
        virtual Vec3_ GetA() = 0;

        /** @brief 获取连接点 B 的世界坐标 */
        virtual Vec3_ GetB() = 0;

        /** @brief 获取连接对象类型 */
        virtual CordObjectType3D ObjectType() = 0;
    };

    /**
     * @brief 两个角度物体间的连接约束（3D） */
    class PhysicsJunctionSS3D : public BaseJunction3D
    {
    public:
        PhysicsAngle3D *mParticle1; // 形状物体1
        Vec3_ mArm1;                // 物体1局部连接臂
        Vec3_ mR1;                  // 物体1世界连接臂
        PhysicsAngle3D *mParticle2; // 形状物体2
        Vec3_ mArm2;                // 物体2局部连接臂
        Vec3_ mR2;                  // 物体2世界连接臂

        PhysicsJunctionSS3D(PhysicsAngle3D *Particle1, Vec3_ arm1,
                            PhysicsAngle3D *Particle2, Vec3_ arm2, CordType3D type);
        ~PhysicsJunctionSS3D();

        void PreStep(FLOAT_ inv_dt) override;
        void ApplyImpulse() override;

        Vec3_ GetA() override { return mParticle1->pos + QuatRotate(mParticle1->rotation, mArm1); }
        Vec3_ GetB() override { return mParticle2->pos + QuatRotate(mParticle2->rotation, mArm2); }
        CordObjectType3D ObjectType() override { return JunctionAA3D; }

#if PhysicsBlock_Serialization3D
        PhysicsJunctionSS3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data) { JsonContrarySerialization(data); };
        void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data) override;
        void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data) override;
#endif
    };

    /**
     * @brief 角度物体到固定点的连接约束（3D） */
    class PhysicsJunctionS3D : public BaseJunction3D
    {
    public:
        Vec3_ mRegularDrop;    // 固定点
        PhysicsAngle3D *mParticle; // 形状物体
        Vec3_ Arm;             // 局部连接臂
        Vec3_ R;               // 世界连接臂

        PhysicsJunctionS3D(PhysicsAngle3D *Particle, Vec3_ arm, Vec3_ RegularDrop, CordType3D type);
        ~PhysicsJunctionS3D();

        void PreStep(FLOAT_ inv_dt) override;
        void ApplyImpulse() override;

        Vec3_ GetA() override { return mParticle->pos + QuatRotate(mParticle->rotation, Arm); }
        Vec3_ GetB() override { return mRegularDrop; }
        CordObjectType3D ObjectType() override { return JunctionA3D; }

#if PhysicsBlock_Serialization3D
        PhysicsJunctionS3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data) { JsonContrarySerialization(data); };
        void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data) override;
        void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data) override;
#endif
    };

    /**
     * @brief 粒子到固定点的连接约束（3D） */
    class PhysicsJunctionP3D : public BaseJunction3D
    {
    public:
        Vec3_ mRegularDrop;          // 固定点
        PhysicsParticle3D *mParticle; // 粒子

        PhysicsJunctionP3D(PhysicsParticle3D *Particle, Vec3_ RegularDrop, CordType3D type);
        ~PhysicsJunctionP3D();

        void PreStep(FLOAT_ inv_dt) override;
        void ApplyImpulse() override;

        Vec3_ GetA() override { return mParticle->pos; }
        Vec3_ GetB() override { return mRegularDrop; }
        CordObjectType3D ObjectType() override { return JunctionP3D; }

#if PhysicsBlock_Serialization3D
        PhysicsJunctionP3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data) { JsonContrarySerialization(data); };
        void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data) override;
        void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data) override;
#endif
    };

    /**
     * @brief 两个粒子间的连接约束（3D） */
    class PhysicsJunctionPP3D : public BaseJunction3D
    {
    public:
        PhysicsParticle3D *mParticle1, *mParticle2;

        PhysicsJunctionPP3D(PhysicsParticle3D *Particle1, PhysicsParticle3D *Particle2, CordType3D type);
        ~PhysicsJunctionPP3D();

        void PreStep(FLOAT_ inv_dt) override;
        void ApplyImpulse() override;

        Vec3_ GetA() override { return mParticle1->pos; }
        Vec3_ GetB() override { return mParticle2->pos; }
        CordObjectType3D ObjectType() override { return JunctionPP3D; }

#if PhysicsBlock_Serialization3D
        PhysicsJunctionPP3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data) { JsonContrarySerialization(data); };
        void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data) override;
        void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data) override;
#endif
    };

}
