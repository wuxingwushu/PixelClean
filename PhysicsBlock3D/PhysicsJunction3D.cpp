#include "PhysicsJunction3D.hpp"
#include "BaseCalculate3D.hpp"

namespace PhysicsBlock3D
{

    namespace
    {
        /**
         * @brief 计算约束法线上的有效质量转动项
         * @param r 世界系连接臂
         * @param n 归一化约束方向
         * @param worldInvI 世界系逆惯量张量
         * @return `(r×n)·I⁻¹·(r×n)`
         * @details 这是 2D `invI * (r×n)²` 的三维推广：2D 中 `I⁻¹` 是标量且
         *          转动轴固定为 Z，故 `(r×n)²` 已含全部信息；3D 中 `I⁻¹` 是
         *          张量，必须把向量 `(r×n)` 夹在两侧。它等价于
         *          `n·((I⁻¹(r×n))×r)`，即求解器用的有效质量形式。 */
        inline FLOAT_ EffectiveAngularMass(const Vec3_ &r, const Vec3_ &n, const Mat3_ &worldInvI)
        {
            const Vec3_ rcn = Cross(r, n);
            return Dot(rcn, worldInvI * rcn);
        }

        /**
         * @brief 计算约束方向并做退化保护
         * @param dp A - B
         * @param outNormal 输出的归一化方向
         * @return 两连接点距离
         * @details 2D 直接 `Normal = dp / L`，两端重合时产生 NaN。
         *          3D 用 SafeNormalize 回退到 +Y，并把距离报为 0。 */
        inline FLOAT_ SafeDirection(const Vec3_ &dp, Vec3_ &outNormal)
        {
            const FLOAT_ L = Modulus(dp);
            outNormal = SafeNormalize(dp, Vec3_(0.0f, 1.0f, 0.0f));
            return L;
        }
    }

    // ═════════════════════════ PhysicsJunctionSS3D ═════════════════════════

    PhysicsJunctionSS3D::PhysicsJunctionSS3D(PhysicsAngle3D *Particle1, Vec3_ arm1,
                                             PhysicsAngle3D *Particle2, Vec3_ arm2, CordType3D type)
        : mParticle1(Particle1), mArm1(arm1), mR1(0.0f),
          mParticle2(Particle2), mArm2(arm2), mR2(0.0f)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    PhysicsJunctionSS3D::~PhysicsJunctionSS3D()
    {
    }

    void PhysicsJunctionSS3D::PreStep(FLOAT_ inv_dt)
    {
        Vec3_ dp = GetA() - GetB();
        FLOAT_ L = SafeDirection(dp, Normal);
        L = L - Length;
        bias = -0.2f * inv_dt * L;
        biasType();

        mR1 = QuatRotate(mParticle1->rotation, mArm1);
        mR2 = QuatRotate(mParticle2->rotation, mArm2);

        // 有效质量：平动 + 两端转动（各向异性惯量，必须用世界系逆张量）
        k = mParticle1->invMass + mParticle2->invMass;
        k += EffectiveAngularMass(mR1, Normal, mParticle1->WorldInvInertia());
        k += EffectiveAngularMass(mR2, Normal, mParticle2->WorldInvInertia());
    }

    void PhysicsJunctionSS3D::ApplyImpulse()
    {
        if (k == 0) return;

        Vec3_ dv = mParticle1->speed + Cross(mParticle1->angleSpeed, mR1)
                 - mParticle2->speed - Cross(mParticle2->angleSpeed, mR2);

        FLOAT_ impulseMagnitude = (Dot(-dv * ElasticityType(), Normal) + bias) / k;
        Vec3_ impulse = Normal * impulseMagnitude;

        mParticle1->speed += mParticle1->invMass * impulse;
        mParticle1->angleSpeed += mParticle1->WorldInvInertia() * Cross(mR1, impulse);

        mParticle2->speed -= mParticle2->invMass * impulse;
        mParticle2->angleSpeed -= mParticle2->WorldInvInertia() * Cross(mR2, impulse);
    }

    // ═════════════════════════ PhysicsJunctionS3D ═════════════════════════

    PhysicsJunctionS3D::PhysicsJunctionS3D(PhysicsAngle3D *Particle, Vec3_ arm, Vec3_ RegularDrop, CordType3D type)
        : mRegularDrop(RegularDrop), mParticle(Particle), Arm(arm), R(0.0f)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    PhysicsJunctionS3D::~PhysicsJunctionS3D()
    {
    }

    void PhysicsJunctionS3D::PreStep(FLOAT_ inv_dt)
    {
        Vec3_ dp = GetA() - GetB();
        FLOAT_ L = SafeDirection(dp, Normal);
        L = L - Length;
        bias = -0.2f * inv_dt * L;
        biasType();

        R = QuatRotate(mParticle->rotation, Arm);

        k = mParticle->invMass;
        k += EffectiveAngularMass(R, Normal, mParticle->WorldInvInertia());
    }

    void PhysicsJunctionS3D::ApplyImpulse()
    {
        if (k == 0) return;

        Vec3_ dv = mParticle->speed + Cross(mParticle->angleSpeed, R);

        FLOAT_ impulseMagnitude = (Dot(-dv * ElasticityType(), Normal) + bias) / k;
        Vec3_ impulse = Normal * impulseMagnitude;

        mParticle->speed += mParticle->invMass * impulse;
        mParticle->angleSpeed += mParticle->WorldInvInertia() * Cross(R, impulse);
    }

    // ═════════════════════════ PhysicsJunctionP3D ═════════════════════════

    PhysicsJunctionP3D::PhysicsJunctionP3D(PhysicsParticle3D *Particle, Vec3_ RegularDrop, CordType3D type)
        : mRegularDrop(RegularDrop), mParticle(Particle)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    PhysicsJunctionP3D::~PhysicsJunctionP3D()
    {
    }

    void PhysicsJunctionP3D::PreStep(FLOAT_ inv_dt)
    {
        Vec3_ dp = GetA() - GetB();
        FLOAT_ L = SafeDirection(dp, Normal);
        L = L - Length;
        bias = -0.2f * inv_dt * L;
        biasType();

        // 粒子无转动，故有效质量只有平动项
        k = mParticle->invMass;
    }

    void PhysicsJunctionP3D::ApplyImpulse()
    {
        if (k == 0) return;

        FLOAT_ impulseMagnitude = (Dot((-mParticle->speed) * ElasticityType(), Normal) + bias) / k;
        Vec3_ impulse = Normal * impulseMagnitude;

        mParticle->speed += mParticle->invMass * impulse;
    }

    // ═════════════════════════ PhysicsJunctionPP3D ═════════════════════════

    PhysicsJunctionPP3D::PhysicsJunctionPP3D(PhysicsParticle3D *Particle1, PhysicsParticle3D *Particle2, CordType3D type)
        : mParticle1(Particle1), mParticle2(Particle2)
    {
        Length = Modulus(GetA() - GetB());
        Type = type;
    }

    PhysicsJunctionPP3D::~PhysicsJunctionPP3D()
    {
    }

    void PhysicsJunctionPP3D::PreStep(FLOAT_ inv_dt)
    {
        Vec3_ dp = GetA() - GetB();
        FLOAT_ L = SafeDirection(dp, Normal);
        L = L - Length;
        bias = -0.2f * inv_dt * L;
        biasType();

        k = mParticle1->invMass + mParticle2->invMass;
    }

    void PhysicsJunctionPP3D::ApplyImpulse()
    {
        if (k == 0) return;

        FLOAT_ impulseMagnitude = (Dot((mParticle2->speed - mParticle1->speed) * ElasticityType(), Normal) + bias) / k;
        Vec3_ impulse = Normal * impulseMagnitude;

        mParticle1->speed += mParticle1->invMass * impulse;
        mParticle2->speed -= mParticle2->invMass * impulse;
    }

    // ═════════════════════════ 序列化 ═════════════════════════

#if PhysicsBlock_Serialization3D

    void BaseJunction3D::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        data["Length"] = Length;
        data["Type"] = static_cast<int>(Type);
        data["k"] = k;
    }

    void BaseJunction3D::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        Length = data["Length"];
        Type = static_cast<CordType3D>(static_cast<int>(data["Type"]));
        k = data["k"];
    }

    void PhysicsJunctionSS3D::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        BaseJunction3D::JsonSerialization(data);
        SerializationVec3(data, mArm1);
        SerializationVec3(data, mArm2);
    }

    void PhysicsJunctionSS3D::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        BaseJunction3D::JsonContrarySerialization(data);
        ContrarySerializationVec3(data, mArm1);
        ContrarySerializationVec3(data, mArm2);
    }

    void PhysicsJunctionS3D::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        BaseJunction3D::JsonSerialization(data);
        SerializationVec3(data, Arm);
        SerializationVec3(data, mRegularDrop);
    }

    void PhysicsJunctionS3D::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        BaseJunction3D::JsonContrarySerialization(data);
        ContrarySerializationVec3(data, Arm);
        ContrarySerializationVec3(data, mRegularDrop);
    }

    void PhysicsJunctionP3D::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        BaseJunction3D::JsonSerialization(data);
        SerializationVec3(data, mRegularDrop);
    }

    void PhysicsJunctionP3D::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        BaseJunction3D::JsonContrarySerialization(data);
        ContrarySerializationVec3(data, mRegularDrop);
    }

    void PhysicsJunctionPP3D::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        BaseJunction3D::JsonSerialization(data);
    }

    void PhysicsJunctionPP3D::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        BaseJunction3D::JsonContrarySerialization(data);
    }

#endif

}
