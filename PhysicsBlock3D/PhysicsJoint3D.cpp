#include "PhysicsJoint3D.hpp"
#include "BaseCalculate3D.hpp"

namespace PhysicsBlock3D
{

    namespace
    {
        /**
         * @brief 由位置向量构造反对称矩阵 Skew(r)
         * @details Skew(r) * v == Cross(r, v)。
         *          2D 没有这个需求（叉积是标量），3D 有效质量矩阵必须用它。
         *          2D 的 K2 = invI*(|r|²E − r⊗r) 正是 Skew(r)·invI·Skew(r)ᵀ，
         *          故 3D 直接写成 `Skew(r) * WorldInvInertia() * Skew(r)ᵀ`
         *          即可自然支持各向异性惯量张量。 */
        inline Mat3_ Skew(const Vec3_ &r)
        {
            return Mat3_(0.0f, -r.z, r.y,
                         r.z, 0.0f, -r.x,
                         -r.y, r.x, 0.0f);
        }

        /**
         * @brief 计算单个物体的转动对有效质量的贡献
         * @details 世界系中 r 处单位角冲量产生的速度变化为
         *          `(I⁻¹(r×P)) × r`，其雅可比为 −Skew(r)，故贡献为
         *          `Skew(r)·I⁻¹·Skew(r)ᵀ`。I⁻¹ 必须是**世界系**张量
         *          （3D 惯量随朝向变化），2D 标量 Izz 无此问题。 */
        inline Mat3_ AngularMassTerm(const Vec3_ &r, const Mat3_ &worldInvI)
        {
            const Mat3_ S = Skew(r);
            return S * worldInvI * glm::transpose(S);
        }
    }

    /** @brief 默认构造函数 */
    PhysicsJoint3D::PhysicsJoint3D(/* args */)
        : M(0.0f), localAnchor1(0.0f), localAnchor2(0.0f), P(0.0f), bias(0.0f),
          biasFactor(0.2f), softness(0.0f), r1(0.0f), r2(0.0f),
          body1(nullptr), body2(nullptr)
    {
    }

    /** @brief 析构函数 */
    PhysicsJoint3D::~PhysicsJoint3D()
    {
    }

    /** @brief 设置关节连接
     *  @param Body1 第一个物体
     *  @param Body2 第二个物体
     *  @param anchor 锚点位置（世界坐标）
     *  @details 计算并存储两个物体在局部坐标系下的锚点位置。
     *           2D 用 `AngleMat::Anticlockwise`（世界→局部）旋转，
     *           3D 对应 `QuatInverseRotate(rotation, …)`。 */
    void PhysicsJoint3D::Set(PhysicsAngle3D *Body1, PhysicsAngle3D *Body2, const Vec3_ anchor)
    {
        body1 = Body1;
        body2 = Body2;

        // 世界系偏移 → 物体局部系（3D 用四元数逆旋转）
        localAnchor1 = QuatInverseRotate(body1->rotation, anchor - body1->pos);
        localAnchor2 = QuatInverseRotate(body2->rotation, anchor - body2->pos);

        // 初始化数值
        P = Vec3_(0.0f);
        softness = 0.0f;
        biasFactor = 0.2f;
    }

    /** @brief 预处理阶段
     *  @param inv_dt 时间步长的倒数
     *  @details 计算世界坐标系下的锚点位置、位置偏差、冲量矩阵，并应用上一次的冲量。
     *           3D 有效质量矩阵（3×3）：
     *             K = (invM1 + invM2)·E
     *               + Skew(r1)·I1⁻¹·Skew(r1)ᵀ
     *               + Skew(r2)·I2⁻¹·Skew(r2)ᵀ
     *           它是 2D 的 `invI*(|r|²E − r⊗r)` 的三维推广，且支持各向异性惯量。 */
    void PhysicsJoint3D::PreStep(FLOAT_ inv_dt)
    {
        // 局部锚点 → 世界系偏移（3D 用四元数旋转）
        r1 = QuatRotate(body1->rotation, localAnchor1);
        r2 = QuatRotate(body2->rotation, localAnchor2);

        // 计算位置偏差
        Vec3_ dp = (body2->pos + r2) - (body1->pos + r1);
        // 计算位置修正速度值
        bias = -biasFactor * inv_dt * dp;

        // 绳子僵硬程度（线速度部分）
        Mat3_ K = Mat3_(body1->invMass + body2->invMass);

        // 两个物体的转动惯量的影响（各向异性，必须用世界系逆张量）
        K += AngularMassTerm(r1, body1->WorldInvInertia());
        K += AngularMassTerm(r2, body2->WorldInvInertia());

        // 柔软度：2D 只加到对角两元，3D 对应加到三个对角元
        K[0][0] += softness;
        K[1][1] += softness;
        K[2][2] += softness;

        // 对称正定求逆（两物体皆静态时 K 为零矩阵，返回零矩阵而非 NaN，
        // 这是相对 2D 用 glm::inverse 的健壮性改进）
        M = SymmetricInverse(K);

        // 使用上一次的冲量
        body1->speed -= body1->invMass * P;
        body1->angleSpeed -= body1->WorldInvInertia() * Cross(r1, P);
        body2->speed += body2->invMass * P;
        body2->angleSpeed += body2->WorldInvInertia() * Cross(r2, P);
    }

    /** @brief 应用冲量阶段
     *  @details 计算速度差，根据冲量矩阵计算冲量并应用到两个物体 */
    void PhysicsJoint3D::ApplyImpulse()
    {
        Vec3_ dv = body2->speed + Cross(body2->angleSpeed, r2)
                 - body1->speed - Cross(body1->angleSpeed, r1);
        Vec3_ impulse = M * (bias - dv - softness * P);

        body1->speed -= body1->invMass * impulse;
        body1->angleSpeed -= body1->WorldInvInertia() * Cross(r1, impulse);
        body2->speed += body2->invMass * impulse;
        body2->angleSpeed += body2->WorldInvInertia() * Cross(r2, impulse);

        P += impulse;
    }

#if PhysicsBlock_Serialization3D
    /** @brief JSON序列化 */
    void PhysicsJoint3D::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        SerializationVec3(data, localAnchor1);
        SerializationVec3(data, localAnchor2);
        SerializationVec3(data, P);
        SerializationVec3(data, bias);
        data["biasFactor"] = biasFactor;
        data["softness"] = softness;
        SerializationMat3(data, M);
    }

    /** @brief JSON反序列化 */
    void PhysicsJoint3D::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        ContrarySerializationVec3(data, localAnchor1);
        ContrarySerializationVec3(data, localAnchor2);
        ContrarySerializationVec3(data, P);
        ContrarySerializationVec3(data, bias);
        biasFactor = data["biasFactor"];
        softness = data["softness"];
        ContrarySerializationMat3(data, M);
    }
#endif

}
