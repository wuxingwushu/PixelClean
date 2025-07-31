#include "PhysicsBaseArbiter.hpp"
#include "BaseCalculate.hpp"
#include <utility>

#define k_biasFactorVAL 0.5 // 位置修正量

namespace PhysicsBlock
{

    // 更新碰撞信息
    void PhysicsBaseArbiterAA::Update(Contact *NewContacts, int numNewContacts)
    {
        for (size_t i = 0; i < numNewContacts; ++i)
        {
            for (size_t j = 0; j < numContacts; ++j)
            {
                if (NewContacts[i].w_side == contacts[j].w_side)
                {
                    NewContacts[i].Pn = contacts[j].Pn;
                    NewContacts[i].Pt = contacts[j].Pt;
                    break;
                }
            }
        }
        for (size_t i = 0; i < numNewContacts; ++i)
        {
            contacts[i].normal = NewContacts[i].normal;
            contacts[i].position = NewContacts[i].position;
            contacts[i].separation = NewContacts[i].separation;
            contacts[i].w_side = NewContacts[i].w_side;
            contacts[i].Pn = NewContacts[i].Pn;
            contacts[i].Pt = NewContacts[i].Pt;
        }

        numContacts = numNewContacts;
    }

    void PhysicsBaseArbiterAA::PreStep()
    {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心 指向碰撞点的 力矩
            c->r2 = c->position - object2->pos; // object2 质心 指向碰撞点的 力矩
        }
    }

    // 预处理
    void PhysicsBaseArbiterAA::PreStep(FLOAT_ inv_dt)
    {
        const FLOAT_ k_allowedPenetration = 0.01;    // 容許穿透
        const FLOAT_ k_biasFactor = k_biasFactorVAL; // 位置修正量

        // 獲取碰撞點
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心 指向碰撞点的 力矩
            c->r2 = c->position - object2->pos; // object2 质心 指向碰撞点的 力矩

            FLOAT_ rn1 = Dot(c->r1, c->normal); // box1质心指向碰撞点 到 法向量 的 投影
            FLOAT_ rn2 = Dot(c->r2, c->normal); // box2质心指向碰撞点 到 法向量 的 投影
            FLOAT_ R1 = Dot(c->r1, c->r1);
            FLOAT_ R2 = Dot(c->r2, c->r2);
            FLOAT_ kNormal = object1->invMass + object2->invMass;
            FLOAT_ kTangent = kNormal;
            kNormal += object1->invMomentInertia * (R1 - rn1 * rn1) + object2->invMomentInertia * (R2 - rn2 * rn2);
            c->massNormal = 1.0 / kNormal;

            Vec2_ tangent = Cross(c->normal, 1.0); // 垂直 normal 的 法向量
            FLOAT_ rt1 = Dot(c->r1, tangent);      // box1质心指向碰撞点 到 垂直法向量 的 投影
            FLOAT_ rt2 = Dot(c->r2, tangent);      // box2质心指向碰撞点 到 垂直法向量 的 投影
            kTangent += object1->invMomentInertia * (R1 - rt1 * rt1) + object2->invMomentInertia * (R2 - rt2 * rt2);
            c->massTangent = 1.0 / kTangent;

            c->bias = -k_biasFactor * inv_dt * std::min(FLOAT_(0.0), c->separation + k_allowedPenetration); // 物体位置修正值大小

            // 施加正常+摩擦脉冲
            Vec2_ P = c->Pn * c->normal + c->Pt * tangent;

            if (object1->invMass != 0)
            {
                object1->speed -= object1->invMass * P;
                object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, P);
            }

            if (object2->invMass != 0)
            {
                object2->speed += object2->invMass * P;
                object2->angleSpeed += object2->invMomentInertia * Cross(c->r2, P);
            }
        }
    }

    // 迭代出结果
    void PhysicsBaseArbiterAA::ApplyImpulse()
    {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;

            // 接触时的相对速度
            Vec2_ dv = object2->speed + Cross(object2->angleSpeed, c->r2) - object1->speed - Cross(object1->angleSpeed, c->r1);

            // 计算法向脉冲
            FLOAT_ vn = Dot(dv, c->normal); // 作用于对方的速度

            FLOAT_ dPn = c->massNormal * (-vn + c->bias); // 移动速度大小修补值

            // 夹紧累积的脉冲
            FLOAT_ Pn0 = c->Pn;
            c->Pn = std::max(Pn0 + dPn, FLOAT_(0.0));
            dPn = c->Pn - Pn0;

            // 应用接触脉冲
            Vec2_ Pn = dPn * c->normal;

            if (object1->invMass != 0)
            {
                object1->speed -= object1->invMass * Pn;
                object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, Pn);
            }

            if (object2->invMass != 0)
            {
                object2->speed += object2->invMass * Pn;
                object2->angleSpeed += object2->invMomentInertia * Cross(c->r2, Pn);
            }

            // 接触时的相对速度
            dv = object2->speed + Cross(object2->angleSpeed, c->r2) - object1->speed - Cross(object1->angleSpeed, c->r1);

            Vec2_ tangent = Cross(c->normal, 1.0);
            FLOAT_ vt = Dot(dv, tangent);        // 作用于对方的角速度
            FLOAT_ dPt = c->massTangent * (-vt); // 旋转速度大小修补值

            // 计算摩擦脉冲
            FLOAT_ maxPt = c->friction * c->Pn;

            // 夹具摩擦
            FLOAT_ oldTangentImpulse = c->Pt;
            c->Pt = Clamp(oldTangentImpulse + dPt, -maxPt, maxPt);
            dPt = c->Pt - oldTangentImpulse;

            // 应用接触脉冲
            Vec2_ Pt = dPt * tangent;

            if (object1->invMass != 0)
            {
                object1->speed -= object1->invMass * Pt;
                object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, Pt);
            }

            if (object2->invMass != 0)
            {
                object2->speed += object2->invMass * Pt;
                object2->angleSpeed += object2->invMomentInertia * Cross(c->r2, Pt);
            }
        }
    }

    // 更新碰撞信息
    void PhysicsBaseArbiterAD::Update(Contact *NewContacts, int numNewContacts)
    {
        for (size_t i = 0; i < numNewContacts; ++i)
        {
            for (size_t j = 0; j < numContacts; ++j)
            {
                if (NewContacts[i].w_side == contacts[j].w_side)
                {
                    NewContacts[i].Pn = contacts[j].Pn;
                    NewContacts[i].Pt = contacts[j].Pt;
                    break;
                }
            }
        }
        for (size_t i = 0; i < numNewContacts; ++i)
        {
            contacts[i].normal = NewContacts[i].normal;
            contacts[i].position = NewContacts[i].position;
            contacts[i].separation = NewContacts[i].separation;
            contacts[i].w_side = NewContacts[i].w_side;
            contacts[i].Pn = NewContacts[i].Pn;
            contacts[i].Pt = NewContacts[i].Pt;
        }

        numContacts = numNewContacts;
    }

    void PhysicsBaseArbiterAD::PreStep()
    {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心 指向碰撞点的 力矩
            c->r2 = {0, 0};                     // object2 质心 指向碰撞点的 力矩
        }
    }

    // 预处理
    void PhysicsBaseArbiterAD::PreStep(FLOAT_ inv_dt)
    {
        const FLOAT_ k_allowedPenetration = 0.01;    // 容許穿透
        const FLOAT_ k_biasFactor = k_biasFactorVAL; // 位置修正量

        // 獲取碰撞點
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心 指向碰撞点的 力矩
            c->r2 = {0, 0};                     // object2 质心 指向碰撞点的 力矩

            FLOAT_ rn1 = Dot(c->r1, c->normal); // box1质心指向碰撞点 到 法向量 的 投影
            FLOAT_ R1 = Dot(c->r1, c->r1);
            FLOAT_ kNormal = object1->invMass + object2->invMass;
            FLOAT_ kTangent = kNormal;
            kNormal += object1->invMomentInertia * (R1 - rn1 * rn1);
            c->massNormal = 1.0 / kNormal;

            Vec2_ tangent = Cross(c->normal, 1.0); // 垂直 normal 的 法向量
            FLOAT_ rt1 = Dot(c->r1, tangent);      // box1质心指向碰撞点 到 垂直法向量 的 投影
            kTangent += object1->invMomentInertia * (R1 - rt1 * rt1);
            c->massTangent = 1.0 / kTangent;

            c->bias = -k_biasFactor * inv_dt * std::min(FLOAT_(0.0), c->separation + k_allowedPenetration); // 物体位置修正值大小

            // 施加正常+摩擦脉冲
            Vec2_ P = c->Pn * c->normal + c->Pt * tangent;

            if (object1->invMass != 0)
            {
                object1->speed -= object1->invMass * P;
                object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, P);
            }

            if (object2->invMass != 0)
            {
                object2->speed += object2->invMass * P;
            }
        }
    }

    // 迭代出结果
    void PhysicsBaseArbiterAD::ApplyImpulse()
    {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;

            // 接触时的相对速度
            Vec2_ dv = object2->speed - object1->speed - Cross(object1->angleSpeed, c->r1);

            // 计算法向脉冲
            FLOAT_ vn = Dot(dv, c->normal); // 作用于对方的速度

            FLOAT_ dPn = c->massNormal * (-vn + c->bias); // 移动速度大小修补值

            // 夹紧累积的脉冲
            FLOAT_ Pn0 = c->Pn;
            c->Pn = std::max(Pn0 + dPn, FLOAT_(0.0));
            dPn = c->Pn - Pn0;

            // 应用接触脉冲
            Vec2_ Pn = dPn * c->normal;
            if (object1->invMass != 0)
            {
                object1->speed -= object1->invMass * Pn;
                object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, Pn);
            }
            if (object2->invMass != 0)
            {
                object2->speed += object2->invMass * Pn;
            }
            // 接触时的相对速度
            dv = object2->speed - object1->speed - Cross(object1->angleSpeed, c->r1);

            Vec2_ tangent = Cross(c->normal, 1.0);
            FLOAT_ vt = Dot(dv, tangent);        // 作用于对方的角速度
            FLOAT_ dPt = c->massTangent * (-vt); // 旋转速度大小修补值

            // 计算摩擦脉冲
            FLOAT_ maxPt = c->friction * c->Pn;

            // 夹具摩擦
            FLOAT_ oldTangentImpulse = c->Pt;
            c->Pt = Clamp(oldTangentImpulse + dPt, -maxPt, maxPt);
            dPt = c->Pt - oldTangentImpulse;

            // 应用接触脉冲
            Vec2_ Pt = dPt * tangent;
            if (object1->invMass != 0)
            {
                object1->speed -= object1->invMass * Pt;
                object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, Pt);
            }
            if (object2->invMass != 0)
            {
                object2->speed += object2->invMass * Pt;
            }
        }
    }

    // 更新碰撞信息
    void PhysicsBaseArbiterA::Update(Contact *NewContacts, int numNewContacts)
    {
        for (size_t i = 0; i < numNewContacts; ++i)
        {
            for (size_t j = 0; j < numContacts; ++j)
            {
                if (NewContacts[i].w_side == contacts[j].w_side)
                {
                    NewContacts[i].Pn = contacts[j].Pn;
                    NewContacts[i].Pt = contacts[j].Pt;
                    break;
                }
            }
        }
        for (size_t i = 0; i < numNewContacts; ++i)
        {
            contacts[i].normal = NewContacts[i].normal;
            contacts[i].position = NewContacts[i].position;
            contacts[i].separation = NewContacts[i].separation;
            contacts[i].w_side = NewContacts[i].w_side;
            contacts[i].Pn = NewContacts[i].Pn;
            contacts[i].Pt = NewContacts[i].Pt;
        }

        numContacts = numNewContacts;
    }

    void PhysicsBaseArbiterA::PreStep()
    {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心 指向碰撞点的 力矩
            c->r2 = {0, 0};
        }
    }

    // 预处理
    void PhysicsBaseArbiterA::PreStep(FLOAT_ inv_dt)
    {
        if (object1->invMass == 0)
            return;

        const FLOAT_ k_allowedPenetration = 0.01;    // 容許穿透
        const FLOAT_ k_biasFactor = k_biasFactorVAL; // 位置修正量

        // 獲取碰撞點
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心 指向碰撞点的 力矩
            c->r2 = {0, 0};

            FLOAT_ rn1 = Dot(c->r1, c->normal); // box1质心指向碰撞点 到 法向量 的 投影
            FLOAT_ R1 = Dot(c->r1, c->r1);
            FLOAT_ kNormal = object1->invMass;
            FLOAT_ kTangent = kNormal;
            kNormal += object1->invMomentInertia * (R1 - rn1 * rn1);
            c->massNormal = 1.0 / kNormal;

            Vec2_ tangent = Cross(c->normal, 1.0); // 垂直 normal 的 法向量
            FLOAT_ rt1 = Dot(c->r1, tangent);      // box1质心指向碰撞点 到 垂直法向量 的 投影
            kTangent += object1->invMomentInertia * (R1 - rt1 * rt1);
            c->massTangent = 1.0 / kTangent;

            c->bias = -k_biasFactor * inv_dt * std::min(FLOAT_(0.0), c->separation + k_allowedPenetration); // 物体位置修正值大小

            // 施加正常+摩擦脉冲
            Vec2_ P = c->Pn * c->normal - c->Pt * tangent;

            object1->speed -= object1->invMass * P;
            object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, P);
        }
    }

    // 迭代出结果
    void PhysicsBaseArbiterA::ApplyImpulse()
    {
        if (object1->invMass == 0)
            return;

        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;

            // 接触时的相对速度
            Vec2_ dv = -object1->speed - Cross(object1->angleSpeed, c->r1);

            // 计算法向脉冲
            FLOAT_ vn = Dot(dv, c->normal); // 作用于对方的速度

            FLOAT_ dPn = c->massNormal * (-vn + c->bias); // 移动速度大小修补值

            // 夹紧累积的脉冲
            FLOAT_ Pn0 = c->Pn;
            c->Pn = std::max(Pn0 + dPn, FLOAT_(0.0));
            dPn = c->Pn - Pn0;

            // 应用接触脉冲
            Vec2_ Pn = dPn * c->normal;

            object1->speed -= object1->invMass * Pn;
            object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, Pn);

            // 接触时的相对速度
            dv = -object1->speed - Cross(object1->angleSpeed, c->r1);

            Vec2_ tangent = Cross(c->normal, -1.0);
            FLOAT_ vt = Dot(dv, tangent);        // 作用于对方的角速度
            FLOAT_ dPt = c->massTangent * (-vt); // 旋转速度大小修补值

            // 计算摩擦脉冲
            FLOAT_ maxPt = c->friction * c->Pn;

            // 夹具摩擦
            FLOAT_ oldTangentImpulse = c->Pt;
            c->Pt = Clamp(oldTangentImpulse + dPt, -maxPt, maxPt);
            dPt = c->Pt - oldTangentImpulse;

            // 应用接触脉冲
            Vec2_ Pt = dPt * tangent;

            object1->speed -= object1->invMass * Pt;
            object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, Pt);
        }
    }

    // 更新碰撞信息
    void PhysicsBaseArbiterD::Update(Contact *NewContacts, int numNewContacts)
    {
        for (size_t i = 0; i < numNewContacts; ++i)
        {
            for (size_t j = 0; j < numContacts; ++j)
            {
                if (NewContacts[i].w_side == contacts[j].w_side)
                {
                    NewContacts[i].Pn = contacts[j].Pn;
                    NewContacts[i].Pt = contacts[j].Pt;
                    break;
                }
            }
        }
        for (size_t i = 0; i < numNewContacts; ++i)
        {
            contacts[i].normal = NewContacts[i].normal;
            contacts[i].position = NewContacts[i].position;
            contacts[i].separation = NewContacts[i].separation;
            contacts[i].w_side = NewContacts[i].w_side;
            contacts[i].Pn = NewContacts[i].Pn;
            contacts[i].Pt = NewContacts[i].Pt;
        }

        numContacts = numNewContacts;
    }

    void PhysicsBaseArbiterD::PreStep()
    {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心 指向碰撞点的 力矩
            c->r2 = {0, 0};
        }
    }

    // 预处理
    void PhysicsBaseArbiterD::PreStep(FLOAT_ inv_dt)
    {
        if (object1->invMass == 0)
            return;

        const FLOAT_ k_allowedPenetration = 0.01;    // 容許穿透
        const FLOAT_ k_biasFactor = k_biasFactorVAL; // 位置修正量

        // 獲取碰撞點
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心 指向碰撞点的 力矩
            c->r2 = {0, 0};

            FLOAT_ rn1 = Dot(c->r1, c->normal); // box1质心指向碰撞点 到 法向量 的 投影
            FLOAT_ R1 = Dot(c->r1, c->r1);
            FLOAT_ kNormal = object1->invMass;
            c->massNormal = 1.0 / kNormal;

            Vec2_ tangent = Cross(c->normal, 1.0); // 垂直 normal 的 法向量
            FLOAT_ rt1 = Dot(c->r1, tangent);      // box1质心指向碰撞点 到 垂直法向量 的 投影
            c->massTangent = 1.0 / kNormal;

            c->bias = -k_biasFactor * inv_dt * std::min(FLOAT_(0.0), c->separation + k_allowedPenetration); // 物体位置修正值大小

            // 施加正常+摩擦脉冲
            Vec2_ P = c->Pn * c->normal + c->Pt * tangent;

            object1->speed -= object1->invMass * P;
        }
    }

    // 迭代出结果
    void PhysicsBaseArbiterD::ApplyImpulse()
    {
        if (object1->invMass == 0)
            return;

        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;

            // 接触时的相对速度
            Vec2_ dv = -object1->speed;

            // 计算法向脉冲
            FLOAT_ vn = Dot(dv, c->normal); // 作用于对方的速度

            FLOAT_ dPn = c->massNormal * (-vn + c->bias); // 移动速度大小修补值

            // 夹紧累积的脉冲
            FLOAT_ Pn0 = c->Pn;
            c->Pn = std::max(Pn0 + dPn, FLOAT_(0.0));
            dPn = c->Pn - Pn0;

            // 应用接触脉冲
            Vec2_ Pn = dPn * c->normal;

            object1->speed -= object1->invMass * Pn;

            // 接触时的相对速度
            dv = -object1->speed;

            Vec2_ tangent = Cross(c->normal, 1.0);
            FLOAT_ vt = Dot(dv, tangent);        // 作用于对方的角速度
            FLOAT_ dPt = c->massTangent * (-vt); // 旋转速度大小修补值

            // 计算摩擦脉冲
            FLOAT_ maxPt = c->friction * c->Pn;

            // 夹具摩擦
            FLOAT_ oldTangentImpulse = c->Pt;
            c->Pt = Clamp(oldTangentImpulse + dPt, -maxPt, maxPt);
            dPt = c->Pt - oldTangentImpulse;

            // 应用接触脉冲
            Vec2_ Pt = dPt * tangent;

            object1->speed -= object1->invMass * Pt;
        }
    }

}