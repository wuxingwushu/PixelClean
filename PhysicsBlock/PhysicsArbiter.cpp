#include "PhysicsArbiter.hpp"
#include "BaseCalculate.hpp"
#include "PhysicsCollide.hpp"
#include <utility>

#define k_biasFactorVAL 0.2 // 位置修正量

namespace PhysicsBlock
{

    PhysicsArbiterSS::PhysicsArbiterSS(PhysicsShape *Object1, PhysicsShape *Object2) : 
        BaseArbiter(Object1, Object2), object1(Object1), object2(Object2)
    {
        numContacts = Collide(contacts, Object1, Object2);
    }

    // 更新碰撞信息
    void PhysicsArbiterSS::Update(Contact *NewContacts, int numNewContacts)
    {
        for (size_t i = 0; i < numNewContacts; ++i)
        {
            for (size_t j = 0; j < numContacts; ++j)
            {
                if(NewContacts[i].w_side == contacts[j].w_side){
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
    
    void PhysicsArbiterSS::PreStep() {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心 指向碰撞点的 力矩
            c->r2 = c->position - object2->pos; // object2 质心 指向碰撞点的 力矩
        }
    }

    // 预处理
    void PhysicsArbiterSS::PreStep(double inv_dt)
    {
        const double k_allowedPenetration = 0.01; // 容許穿透
        const double k_biasFactor = k_biasFactorVAL;          // 位置修正量

        // 獲取碰撞點
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心 指向碰撞点的 力矩
            c->r2 = c->position - object2->pos; // object2 质心 指向碰撞点的 力矩

            double rn1 = Dot(c->r1, c->normal); // box1质心指向碰撞点 到 法向量 的 投影
            double rn2 = Dot(c->r2, c->normal); // box2质心指向碰撞点 到 法向量 的 投影
            double R1 = Dot(c->r1, c->r1);
            double R2 = Dot(c->r2, c->r2);
            double kNormal = object1->invMass + object2->invMass;
            double kTangent = kNormal;
            kNormal += object1->invMomentInertia * (R1 - rn1 * rn1) + object2->invMomentInertia * (R2 - rn2 * rn2);
            c->massNormal = 1.0 / kNormal;

            glm::dvec2 tangent = Cross(c->normal, 1.0); // 垂直 normal 的 法向量
            double rt1 = Dot(c->r1, tangent);           // box1质心指向碰撞点 到 垂直法向量 的 投影
            double rt2 = Dot(c->r2, tangent);           // box2质心指向碰撞点 到 垂直法向量 的 投影
            kTangent += object1->invMomentInertia * (R1 - rt1 * rt1) + object2->invMomentInertia * (R2 - rt2 * rt2);
            c->massTangent = 1.0 / kTangent;

            c->bias = -k_biasFactor * inv_dt * std::min(0.0, c->separation + k_allowedPenetration); // 物体位置修正值大小

            // 施加正常+摩擦脉冲
            glm::dvec2 P = c->Pn * c->normal + c->Pt * tangent;

            object1->speed -= object1->invMass * P;
            object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, P);

            object2->speed += object2->invMass * P;
            object2->angleSpeed += object2->invMomentInertia * Cross(c->r2, P);
        }
    }

    // 迭代出结果
    void PhysicsArbiterSS::ApplyImpulse()
    {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;

            // 接触时的相对速度
            glm::dvec2 dv = object2->speed + Cross(object2->angleSpeed, c->r2) - object1->speed - Cross(object1->angleSpeed, c->r1);

            // 计算法向脉冲
            double vn = Dot(dv, c->normal); // 作用于对方的速度

            double dPn = c->massNormal * (-vn + c->bias); // 移动速度大小修补值

            // 夹紧累积的脉冲
            double Pn0 = c->Pn;
            c->Pn = std::max(Pn0 + dPn, 0.0);
            dPn = c->Pn - Pn0;

            // 应用接触脉冲
            glm::dvec2 Pn = dPn * c->normal;

            object1->speed -= object1->invMass * Pn;
            object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, Pn);

            object2->speed += object2->invMass * Pn;
            object2->angleSpeed += object2->invMomentInertia * Cross(c->r2, Pn);

            // 接触时的相对速度
            dv = object2->speed + Cross(object2->angleSpeed, c->r2) - object1->speed - Cross(object1->angleSpeed, c->r1);

            glm::dvec2 tangent = Cross(c->normal, 1.0);
            double vt = Dot(dv, tangent);        // 作用于对方的角速度
            double dPt = c->massTangent * (-vt); // 旋转速度大小修补值

            // 计算摩擦脉冲
            double maxPt = friction * c->Pn;

            // 夹具摩擦
            double oldTangentImpulse = c->Pt;
            c->Pt = Clamp(oldTangentImpulse + dPt, -maxPt, maxPt);
            dPt = c->Pt - oldTangentImpulse;

            // 应用接触脉冲
            glm::dvec2 Pt = dPt * tangent;

            object1->speed -= object1->invMass * Pt;
            object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, Pt);

            object2->speed += object2->invMass * Pt;
            object2->angleSpeed += object2->invMomentInertia * Cross(c->r2, Pt);
        }
    }

    PhysicsArbiterSP::PhysicsArbiterSP(PhysicsShape *Object1, PhysicsParticle *Object2) : 
        BaseArbiter(Object1, Object2), object1(Object1), object2(Object2)
    {
        numContacts = Collide(contacts, Object1, Object2);
    }

    // 更新碰撞信息
    void PhysicsArbiterSP::Update(Contact *NewContacts, int numNewContacts)
    {
        for (size_t i = 0; i < numNewContacts; ++i)
        {
            for (size_t j = 0; j < numContacts; ++j)
            {
                if(NewContacts[i].w_side == contacts[j].w_side){
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

    void PhysicsArbiterSP::PreStep() {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心 指向碰撞点的 力矩
            c->r2 = {0,0}; // object2 质心 指向碰撞点的 力矩
        }
    }

    // 预处理
    void PhysicsArbiterSP::PreStep(double inv_dt)
    {
        const double k_allowedPenetration = 0.01; // 容許穿透
        const double k_biasFactor = k_biasFactorVAL;          // 位置修正量

        // 獲取碰撞點
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心 指向碰撞点的 力矩
            c->r2 = {0,0}; // object2 质心 指向碰撞点的 力矩

            double rn1 = Dot(c->r1, c->normal); // box1质心指向碰撞点 到 法向量 的 投影
            double R1 = Dot(c->r1, c->r1);
            double kNormal = object1->invMass + object2->invMass;
            double kTangent = kNormal;
            kNormal += object1->invMomentInertia * (R1 - rn1 * rn1);
            c->massNormal = 1.0 / kNormal;

            glm::dvec2 tangent = Cross(c->normal, 1.0); // 垂直 normal 的 法向量
            double rt1 = Dot(c->r1, tangent);           // box1质心指向碰撞点 到 垂直法向量 的 投影
            kTangent += object1->invMomentInertia * (R1 - rt1 * rt1);
            c->massTangent = 1.0 / kTangent;

            c->bias = -k_biasFactor * inv_dt * std::min(0.0, c->separation + k_allowedPenetration); // 物体位置修正值大小

            // 施加正常+摩擦脉冲
            glm::dvec2 P = c->Pn * c->normal + c->Pt * tangent;

            object1->speed -= object1->invMass * P;
            object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, P);
            // object1->PFAngleSpeed() -= (object1->PFGetInvI() * Cross(c->r1, P) + object2->PFGetInvI() * Cross(c->r2, P));

            object2->speed += object2->invMass * P;
            // object2->PFAngleSpeed() += object2->PFGetInvI() * Cross(c->r2, P);
        }
    }

    // 迭代出结果
    void PhysicsArbiterSP::ApplyImpulse()
    {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;

            // 接触时的相对速度
            glm::dvec2 dv = object2->speed - object1->speed - Cross(object1->angleSpeed, c->r1);

            // 计算法向脉冲
            double vn = Dot(dv, c->normal); // 作用于对方的速度

            double dPn = c->massNormal * (-vn + c->bias); // 移动速度大小修补值

            // 夹紧累积的脉冲
            double Pn0 = c->Pn;
            c->Pn = std::max(Pn0 + dPn, 0.0);
            dPn = c->Pn - Pn0;

            // 应用接触脉冲
            glm::dvec2 Pn = dPn * c->normal;

            object1->speed -= object1->invMass * Pn;
            object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, Pn);

            object2->speed += object2->invMass * Pn;
            // object2->PFAngleSpeed() += object2->PFGetInvI() * Cross(c->r2, Pn);

            // 接触时的相对速度
            dv = object2->speed - object1->speed - Cross(object1->angleSpeed, c->r1);

            glm::dvec2 tangent = Cross(c->normal, 1.0);
            double vt = Dot(dv, tangent);        // 作用于对方的角速度
            double dPt = c->massTangent * (-vt); // 旋转速度大小修补值

            // 计算摩擦脉冲
            double maxPt = friction * c->Pn;

            // 夹具摩擦
            double oldTangentImpulse = c->Pt;
            c->Pt = Clamp(oldTangentImpulse + dPt, -maxPt, maxPt);
            dPt = c->Pt - oldTangentImpulse;

            // 应用接触脉冲
            glm::dvec2 Pt = dPt * tangent;

            object1->speed -= object1->invMass * Pt;
            object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, Pt);

            object2->speed += object2->invMass * Pt;
            // object2->PFAngleSpeed() += object2->PFGetInvI() * Cross(c->r2, Pt);
        }
    }

    PhysicsArbiterS::PhysicsArbiterS(PhysicsShape *Object1, MapFormwork *Object2) : 
        BaseArbiter(Object1, Object2), object1(Object1), object2(Object2)
    {
        numContacts = Collide(contacts, Object1, Object2);
    }

    // 更新碰撞信息
    void PhysicsArbiterS::Update(Contact *NewContacts, int numNewContacts)
    {
        for (size_t i = 0; i < numNewContacts; ++i)
        {
            for (size_t j = 0; j < numContacts; ++j)
            {
                if(NewContacts[i].w_side == contacts[j].w_side){
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

    void PhysicsArbiterS::PreStep() {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心 指向碰撞点的 力矩
            c->r2 = { 0, 0 };
        }
    }

    // 预处理
    void PhysicsArbiterS::PreStep(double inv_dt)
    {
        const double k_allowedPenetration = 0.01; // 容許穿透
        const double k_biasFactor = k_biasFactorVAL;          // 位置修正量

        // 獲取碰撞點
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心 指向碰撞点的 力矩
            c->r2 = { 0, 0 };

            double rn1 = Dot(c->r1, c->normal); // box1质心指向碰撞点 到 法向量 的 投影
            double R1 = Dot(c->r1, c->r1);
            double kNormal = object1->invMass;
            double kTangent = kNormal;
            kNormal += object1->invMomentInertia * (R1 - rn1 * rn1);
            c->massNormal = 1.0 / kNormal;

            glm::dvec2 tangent = Cross(c->normal, 1.0); // 垂直 normal 的 法向量
            double rt1 = Dot(c->r1, tangent);           // box1质心指向碰撞点 到 垂直法向量 的 投影
            kTangent += object1->invMomentInertia * (R1 - rt1 * rt1);
            c->massTangent = 1.0 / kTangent;

            c->bias = -k_biasFactor * inv_dt * std::min(0.0, c->separation + k_allowedPenetration); // 物体位置修正值大小

            // 施加正常+摩擦脉冲
            glm::dvec2 P = c->Pn * c->normal - c->Pt * tangent;

            object1->speed -= object1->invMass * P;
            object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, P);
        }
    }

    // 迭代出结果
    void PhysicsArbiterS::ApplyImpulse()
    {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;

            // 接触时的相对速度
            glm::dvec2 dv = -object1->speed - Cross(object1->angleSpeed, c->r1);

            // 计算法向脉冲
            double vn = Dot(dv, c->normal); // 作用于对方的速度

            double dPn = c->massNormal * (-vn + c->bias); // 移动速度大小修补值

            // 夹紧累积的脉冲
            double Pn0 = c->Pn;
            c->Pn = std::max(Pn0 + dPn, 0.0);
            dPn = c->Pn - Pn0;

            // 应用接触脉冲
            glm::dvec2 Pn = dPn * c->normal;

            object1->speed -= object1->invMass * Pn;
            object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, Pn);

            // 接触时的相对速度
            dv = -object1->speed - Cross(object1->angleSpeed, c->r1);

            glm::dvec2 tangent = Cross(c->normal, -1.0);
            double vt = Dot(dv, tangent);        // 作用于对方的角速度
            double dPt = c->massTangent * (-vt); // 旋转速度大小修补值

            // 计算摩擦脉冲
            double maxPt = friction * c->Pn;

            // 夹具摩擦
            double oldTangentImpulse = c->Pt;
            c->Pt = Clamp(oldTangentImpulse + dPt, -maxPt, maxPt);
            dPt = c->Pt - oldTangentImpulse;

            // 应用接触脉冲
            glm::dvec2 Pt = dPt * tangent;

            object1->speed -= object1->invMass * Pt;
            object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, Pt);
        }
    }

    PhysicsArbiterP::PhysicsArbiterP(PhysicsParticle *Object1, MapFormwork *Object2) : 
        BaseArbiter(Object1, Object2), object1(Object1), object2(Object2)
    {
        numContacts = Collide(contacts, Object1, Object2);
    }

    // 更新碰撞信息
    void PhysicsArbiterP::Update(Contact *NewContacts, int numNewContacts)
    {
        for (size_t i = 0; i < numNewContacts; ++i)
        {
            for (size_t j = 0; j < numContacts; ++j)
            {
                if(NewContacts[i].w_side == contacts[j].w_side){
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

    void PhysicsArbiterP::PreStep() {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心 指向碰撞点的 力矩
            c->r2 = { 0, 0 };
        }
    }

    // 预处理
    void PhysicsArbiterP::PreStep(double inv_dt)
    {
        const double k_allowedPenetration = 0.01; // 容許穿透
        const double k_biasFactor = k_biasFactorVAL;          // 位置修正量

        // 獲取碰撞點
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心 指向碰撞点的 力矩
            c->r2 = { 0, 0 };

            double rn1 = Dot(c->r1, c->normal); // box1质心指向碰撞点 到 法向量 的 投影
            double R1 = Dot(c->r1, c->r1);
            double kNormal = object1->invMass;
            c->massNormal = 1.0 / kNormal;

            glm::dvec2 tangent = Cross(c->normal, 1.0); // 垂直 normal 的 法向量
            double rt1 = Dot(c->r1, tangent);           // box1质心指向碰撞点 到 垂直法向量 的 投影
            c->massTangent = 1.0 / kNormal;

            c->bias = -k_biasFactor * inv_dt * std::min(0.0, c->separation + k_allowedPenetration); // 物体位置修正值大小

            // 施加正常+摩擦脉冲
            glm::dvec2 P = c->Pn * c->normal + c->Pt * tangent;

            object1->speed -= object1->invMass * P;
        }
    }

    // 迭代出结果
    void PhysicsArbiterP::ApplyImpulse()
    {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;

            // 接触时的相对速度
            glm::dvec2 dv = -object1->speed;

            // 计算法向脉冲
            double vn = Dot(dv, c->normal); // 作用于对方的速度

            double dPn = c->massNormal * (-vn + c->bias); // 移动速度大小修补值

            // 夹紧累积的脉冲
            double Pn0 = c->Pn;
            c->Pn = std::max(Pn0 + dPn, 0.0);
            dPn = c->Pn - Pn0;

            // 应用接触脉冲
            glm::dvec2 Pn = dPn * c->normal;

            object1->speed -= object1->invMass * Pn;

            // 接触时的相对速度
            dv = -object1->speed;

            glm::dvec2 tangent = Cross(c->normal, 1.0);
            double vt = Dot(dv, tangent);        // 作用于对方的角速度
            double dPt = c->massTangent * (-vt); // 旋转速度大小修补值

            // 计算摩擦脉冲
            double maxPt = friction * c->Pn;

            // 夹具摩擦
            double oldTangentImpulse = c->Pt;
            c->Pt = Clamp(oldTangentImpulse + dPt, -maxPt, maxPt);
            dPt = c->Pt - oldTangentImpulse;

            // 应用接触脉冲
            glm::dvec2 Pt = dPt * tangent;

            object1->speed -= object1->invMass * Pt;
        }
    }

}