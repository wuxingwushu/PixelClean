#include "PhysicsJoint.hpp"
#include "BaseCalculate.hpp"


namespace PhysicsBlock
{

    PhysicsJoint::PhysicsJoint(/* args */)
    {
    }

    PhysicsJoint::~PhysicsJoint()
    {
    }

    void PhysicsJoint::Set(PhysicsShape *Body1, PhysicsShape *Body2, const Vec2_ anchor)
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

}
