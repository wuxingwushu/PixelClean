#include "PhysicsJoint.hpp"
#include "BaseCalculate.hpp"


namespace PhysicsBlock
{

    /** @brief 默认构造函数 */
    PhysicsJoint::PhysicsJoint(/* args */)
    {
    }

    /** @brief 析构函数 */
    PhysicsJoint::~PhysicsJoint()
    {
    }

    /** @brief 设置关节连接
     *  @param Body1 第一个物体
     *  @param Body2 第二个物体
     *  @param anchor 锚点位置（世界坐标）
     *  @details 计算并存储两个物体在局部坐标系下的锚点位置 */
    void PhysicsJoint::Set(PhysicsAngle *Body1, PhysicsAngle *Body2, const Vec2_ anchor)
    {
        body1 = Body1;
        body2 = Body2;

        AngleMat AngleMat1(body1->angle);
        AngleMat AngleMat2(body2->angle);

        // 获取对应 0角度 杠杆的长度角度
        localAnchor1 = AngleMat1.Anticlockwise(anchor - body1->pos);
        localAnchor2 = AngleMat2.Anticlockwise(anchor - body2->pos);

        // 初始化数值
        P = { 0, 0 };
        softness = 0.0;
	    biasFactor = 0.2;
    }

    /** @brief 预处理阶段
     *  @param inv_dt 时间步长的倒数
     *  @details 计算世界坐标系下的锚点位置、位置偏差、冲量矩阵，并应用上一次的冲量 */
    void PhysicsJoint::PreStep(FLOAT_ inv_dt)
    {
        AngleMat AngleMat1(body1->angle);
        AngleMat AngleMat2(body2->angle);

        r1 = AngleMat1.Rotary(localAnchor1);
        r2 = AngleMat2.Rotary(localAnchor2);
        
        // 计算位置偏差
        Vec2_ dp = (body2->pos + r2) - (body1->pos + r1);
        // 计算位置修正速度值
        bias = -biasFactor * inv_dt * dp;


        // 绳子僵硬程度
        Mat2_ K1;
        K1[0][0] = body1->invMass + body2->invMass;
        K1[0][1] = 0;
        K1[1][0] = 0;
        K1[1][1] = body1->invMass + body2->invMass;
        // 物体1 的 转动惯量的影响
        Mat2_ K2;
        K2[0][0] = body1->invMomentInertia * r1.y * r1.y;
        K2[0][1] = -body1->invMomentInertia * r1.x * r1.y;
        K2[1][0] = -body1->invMomentInertia * r1.x * r1.y;
        K2[1][1] = body1->invMomentInertia * r1.x * r1.x;
        // 物体2 的 转动惯量的影响
        Mat2_ K3;
        K3[0][0] = body2->invMomentInertia * r2.y * r2.y;
        K3[0][1] = -body2->invMomentInertia * r2.x * r2.y;
        K3[1][0] = -body2->invMomentInertia * r2.x * r2.y;
        K3[1][1] = body2->invMomentInertia * r2.x * r2.x;

        Mat2_ K = K1 + K2 + K3;
	    K[0][0] += softness;
	    K[1][1] += softness;
	    M = glm::inverse(K);
        
        // 使用上一次的冲量
        body1->speed -= body1->invMass * P;
		body1->angleSpeed -= body1->invMomentInertia * Cross(r1, P);
		body2->speed += body2->invMass * P;
		body2->angleSpeed += body2->invMomentInertia * Cross(r2, P);
    }

    /** @brief 应用冲量阶段
     *  @details 计算速度差，根据冲量矩阵计算冲量并应用到两个物体 */
    void PhysicsJoint::ApplyImpulse()
    {
        Vec2_ dv = body2->speed + Cross(body2->angleSpeed, r2) - body1->speed - Cross(body1->angleSpeed, r1);
        Vec2_ impulse = M * (bias - dv - softness * P);

        body1->speed -= body1->invMass * impulse;
		body1->angleSpeed -= body1->invMomentInertia * Cross(r1, impulse);
		body2->speed += body2->invMass * impulse;
		body2->angleSpeed += body2->invMomentInertia * Cross(r2, impulse);

        P += impulse;
    }

#if PhysicsBlock_Serialization
    /** @brief JSON序列化 */
    void PhysicsJoint::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        SerializationVec2(data, localAnchor1);
        SerializationVec2(data, localAnchor2);
        SerializationVec2(data, P);
        SerializationVec2(data, bias);
        data["biasFactor"] = biasFactor;
        data["softness"] = softness;
        data["M1"] = M[0][0];
        data["M2"] = M[1][0];
        data["M3"] = M[0][1];
        data["M4"] = M[1][1];
    }

    /** @brief JSON反序列化 */
    void PhysicsJoint::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
    {
        ContrarySerializationVec2(data, localAnchor1);
        ContrarySerializationVec2(data, localAnchor2);
        ContrarySerializationVec2(data, P);
        ContrarySerializationVec2(data, bias);
        biasFactor = data["biasFactor"];
        softness = data["softness"];
        M[0][0] = data["M1"];
        M[1][0] = data["M2"];
        M[0][1] = data["M3"];
        M[1][1] = data["M4"];
    }
#endif

}