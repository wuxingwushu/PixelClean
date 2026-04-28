#include "PhysicsBaseArbiter.hpp"
#include "BaseCalculate.hpp"
#include <utility>

#define k_biasFactorVAL 0.5 // 位置修正量系数

namespace PhysicsBlock
{

    // ========== PhysicsBaseArbiterAA 实现 ==========

    /**
     * @brief   更新碰撞信息
     * @param   NewContacts 新的碰撞点数组
     * @param   numNewContacts 新碰撞点数量
     * @details 将新碰撞信息复制到内部，并保留之前帧的冲量累积值（Pn和Pt）
     */
    void PhysicsBaseArbiterAA::Update(Contact *NewContacts, int numNewContacts)
    {
        // 保留之前的冲量累积值
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
        
        // 更新碰撞点信息
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

    /**
     * @brief   预处理（无时间步长版本）
     * @details 计算碰撞点相对于两个物体质心的位置向量
     */
    void PhysicsBaseArbiterAA::PreStep()
    {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心指向碰撞点的向量
            c->r2 = c->position - object2->pos; // object2 质心指向碰撞点的向量
        }
    }

    /**
     * @brief   预处理（带时间步长版本）
     * @param   inv_dt 时间步长的倒数
     * @details 计算有效质量、位置修正值，并应用累积冲量到物体上
     */
    void PhysicsBaseArbiterAA::PreStep(FLOAT_ inv_dt)
    {
        const FLOAT_ k_allowedPenetration = 0.01;    // 允许穿透距离
        const FLOAT_ k_biasFactor = k_biasFactorVAL; // 位置修正系数

        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心指向碰撞点的向量
            c->r2 = c->position - object2->pos; // object2 质心指向碰撞点的向量

            // 计算法向有效质量
            FLOAT_ rn1 = Dot(c->r1, c->normal);   // r1 在法向量上的投影
            FLOAT_ rn2 = Dot(c->r2, c->normal);   // r2 在法向量上的投影
            FLOAT_ R1 = Dot(c->r1, c->r1);        // r1 的模长平方
            FLOAT_ R2 = Dot(c->r2, c->r2);        // r2 的模长平方
            FLOAT_ kNormal = object1->invMass + object2->invMass;
            FLOAT_ kTangent = kNormal;
            
            // 考虑旋转惯量的影响
            kNormal += object1->invMomentInertia * (R1 - rn1 * rn1) + object2->invMomentInertia * (R2 - rn2 * rn2);
            c->massNormal = 1.0 / kNormal;

            // 计算切向有效质量
            Vec2_ tangent = Cross(c->normal, 1.0); // 垂直于法向量的切向量
            FLOAT_ rt1 = Dot(c->r1, tangent);     // r1 在切向量上的投影
            FLOAT_ rt2 = Dot(c->r2, tangent);     // r2 在切向量上的投影
            kTangent += object1->invMomentInertia * (R1 - rt1 * rt1) + object2->invMomentInertia * (R2 - rt2 * rt2);
            c->massTangent = 1.0 / kTangent;

            // 计算位置修正值（用于穿透修复）
            c->bias = -k_biasFactor * inv_dt * std::min(FLOAT_(0.0), c->separation + k_allowedPenetration);

            // 应用累积的冲量（法向 + 切向）
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

    /**
     * @brief   应用冲量
     * @details 根据碰撞信息计算并应用法向冲量和切向冲量（摩擦力）
     */
    void PhysicsBaseArbiterAA::ApplyImpulse()
    {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;

            // 计算接触点的相对速度
            Vec2_ dv = object2->speed + Cross(object2->angleSpeed, c->r2) 
                     - object1->speed - Cross(object1->angleSpeed, c->r1);

            // 计算法向冲量
            FLOAT_ vn = Dot(dv, c->normal);          // 相对速度在法向的分量
            FLOAT_ dPn = c->massNormal * (-vn + c->bias); // 需要施加的法向冲量

            // 夹紧累积冲量（防止负冲量）
            FLOAT_ Pn0 = c->Pn;
            c->Pn = std::max(Pn0 + dPn, FLOAT_(0.0));
            dPn = c->Pn - Pn0;

            // 应用法向冲量
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

            // 重新计算相对速度（法向冲量已改变速度）
            dv = object2->speed + Cross(object2->angleSpeed, c->r2) 
               - object1->speed - Cross(object1->angleSpeed, c->r1);

            // 计算切向冲量（摩擦力）
            Vec2_ tangent = Cross(c->normal, 1.0);
            FLOAT_ vt = Dot(dv, tangent);          // 相对速度在切向的分量
            FLOAT_ dPt = c->massTangent * (-vt);   // 需要施加的切向冲量

            // 计算最大摩擦力（受法向力限制）
            FLOAT_ maxPt = c->friction * c->Pn;

            // 夹紧切向冲量
            FLOAT_ oldTangentImpulse = c->Pt;
            c->Pt = Clamp(oldTangentImpulse + dPt, -maxPt, maxPt);
            dPt = c->Pt - oldTangentImpulse;

            // 应用切向冲量
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

    // ========== PhysicsBaseArbiterAD 实现（动态形状 + 动态点）==========

    /**
     * @brief   更新碰撞信息
     * @param   NewContacts 新的碰撞点数组
     * @param   numNewContacts 新碰撞点数量
     * @details 将新碰撞信息复制到内部，并保留之前帧的冲量累积值（Pn和Pt）
     *          点对象没有旋转，因此不涉及旋转相关的计算
     */
    void PhysicsBaseArbiterAD::Update(Contact *NewContacts, int numNewContacts)
    {
        // 保留之前的冲量累积值（用于帧间冲量缓存）
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
        // 更新碰撞点信息（法向量、位置、分离距离、接触面标识、冲量累积）
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

    /**
     * @brief   预处理（无时间步长版本）
     * @details 计算碰撞点相对于物体质心的位置向量
     *          点对象（object2）没有旋转，所以 r2 始终为零向量
     */
    void PhysicsBaseArbiterAD::PreStep()
    {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心指向碰撞点的向量
            c->r2 = {0, 0};                     // 点对象没有旋转半径
        }
    }

    /**
     * @brief   预处理（带时间步长版本）
     * @param   inv_dt 时间步长的倒数
     * @details 计算有效质量、位置修正值，并应用累积冲量到物体上
     *          点对象（object2）没有旋转，所以只计算 object1 的旋转惯量影响
     */
    void PhysicsBaseArbiterAD::PreStep(FLOAT_ inv_dt)
    {
        const FLOAT_ k_allowedPenetration = 0.01;    // 允许穿透距离
        const FLOAT_ k_biasFactor = k_biasFactorVAL; // 位置修正系数

        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos;
            c->r2 = {0, 0};

            // 计算法向有效质量
            FLOAT_ rn1 = Dot(c->r1, c->normal);   // r1 在法向量上的投影
            FLOAT_ R1 = Dot(c->r1, c->r1);        // r1 的模长平方
            FLOAT_ kNormal = object1->invMass + object2->invMass;
            FLOAT_ kTangent = kNormal;
            
            // 只考虑 object1 的旋转惯量（object2 是点，没有旋转）
            kNormal += object1->invMomentInertia * (R1 - rn1 * rn1);
            c->massNormal = 1.0 / kNormal;

            // 计算切向有效质量
            Vec2_ tangent = Cross(c->normal, 1.0); // 垂直于法向量的切向量
            FLOAT_ rt1 = Dot(c->r1, tangent);     // r1 在切向量上的投影
            kTangent += object1->invMomentInertia * (R1 - rt1 * rt1);
            c->massTangent = 1.0 / kTangent;

            // 计算位置修正值（用于穿透修复）
            c->bias = -k_biasFactor * inv_dt * std::min(FLOAT_(0.0), c->separation + k_allowedPenetration);

            // 应用累积的冲量（法向 + 切向）
            Vec2_ P = c->Pn * c->normal + c->Pt * tangent;

            if (object1->invMass != 0)
            {
                object1->speed -= object1->invMass * P;
                object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, P);
            }
            if (object2->invMass != 0)
            {
                object2->speed += object2->invMass * P;
                // object2 是点，没有角速度
            }
        }
    }

    /**
     * @brief   应用冲量
     * @details 根据碰撞信息计算并应用法向冲量和切向冲量（摩擦力）
     *          点对象（object2）没有旋转，所以只更新其线速度
     */
    void PhysicsBaseArbiterAD::ApplyImpulse()
    {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;

            // 计算接触点的相对速度（点对象没有旋转速度）
            Vec2_ dv = object2->speed - object1->speed - Cross(object1->angleSpeed, c->r1);

            // 计算法向冲量
            FLOAT_ vn = Dot(dv, c->normal);          // 相对速度在法向的分量
            FLOAT_ dPn = c->massNormal * (-vn + c->bias); // 需要施加的法向冲量

            // 夹紧累积冲量（防止负冲量）
            FLOAT_ Pn0 = c->Pn;
            c->Pn = std::max(Pn0 + dPn, FLOAT_(0.0));
            dPn = c->Pn - Pn0;

            // 应用法向冲量
            Vec2_ Pn = dPn * c->normal;
            if (object1->invMass != 0)
            {
                object1->speed -= object1->invMass * Pn;
                object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, Pn);
            }
            if (object2->invMass != 0)
            {
                object2->speed += object2->invMass * Pn;
                // object2 是点，没有角速度需要更新
            }

            // 重新计算相对速度（法向冲量已改变速度）
            dv = object2->speed - object1->speed - Cross(object1->angleSpeed, c->r1);

            // 计算切向冲量（摩擦力）
            Vec2_ tangent = Cross(c->normal, 1.0);
            FLOAT_ vt = Dot(dv, tangent);          // 相对速度在切向的分量
            FLOAT_ dPt = c->massTangent * (-vt);   // 需要施加的切向冲量

            // 计算最大摩擦力（受法向力限制）
            FLOAT_ maxPt = c->friction * c->Pn;

            // 夹紧切向冲量
            FLOAT_ oldTangentImpulse = c->Pt;
            c->Pt = Clamp(oldTangentImpulse + dPt, -maxPt, maxPt);
            dPt = c->Pt - oldTangentImpulse;

            // 应用切向冲量
            Vec2_ Pt = dPt * tangent;
            if (object1->invMass != 0)
            {
                object1->speed -= object1->invMass * Pt;
                object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, Pt);
            }
            if (object2->invMass != 0)
            {
                object2->speed += object2->invMass * Pt;
                // object2 是点，没有角速度需要更新
            }
        }
    }

    // ========== PhysicsBaseArbiterA 实现（动态形状 + 地图）==========

    /**
     * @brief   更新碰撞信息
     * @param   NewContacts 新的碰撞点数组
     * @param   numNewContacts 新碰撞点数量
     * @details 将新碰撞信息复制到内部，并保留之前帧的冲量累积值（Pn和Pt）
     *          地图是静态的，所以不涉及地图对象的速度更新
     */
    void PhysicsBaseArbiterA::Update(Contact *NewContacts, int numNewContacts)
    {
        // 保留之前的冲量累积值（用于帧间冲量缓存）
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
        // 更新碰撞点信息（法向量、位置、分离距离、接触面标识、冲量累积）
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

    /**
     * @brief   预处理（无时间步长版本）
     * @details 计算碰撞点相对于物体质心的位置向量
     *          地图是静态的，所以 r2 始终为零向量
     */
    void PhysicsBaseArbiterA::PreStep()
    {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // object1 质心指向碰撞点的向量
            c->r2 = {0, 0};                     // 地图是静态的，没有质心偏移
        }
    }

    /**
     * @brief   预处理（带时间步长版本）
     * @param   inv_dt 时间步长的倒数
     * @details 计算有效质量、位置修正值，并应用累积冲量到物体上
     *          地图是静态的（质量无穷大），所以只计算 object1 的有效质量
     */
    void PhysicsBaseArbiterA::PreStep(FLOAT_ inv_dt)
    {
        // 如果 object1 质量无穷大（固定物体），无需处理
        if (object1->invMass == 0)
            return;

        const FLOAT_ k_allowedPenetration = 0.01;    // 允许穿透距离
        const FLOAT_ k_biasFactor = k_biasFactorVAL; // 位置修正系数

        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos;
            c->r2 = {0, 0};

            // 计算法向有效质量（只考虑 object1，地图质量无穷大）
            FLOAT_ rn1 = Dot(c->r1, c->normal);   // r1 在法向量上的投影
            FLOAT_ R1 = Dot(c->r1, c->r1);        // r1 的模长平方
            FLOAT_ kNormal = object1->invMass;
            FLOAT_ kTangent = kNormal;
            
            // 考虑 object1 的旋转惯量影响
            kNormal += object1->invMomentInertia * (R1 - rn1 * rn1);
            c->massNormal = 1.0 / kNormal;

            // 计算切向有效质量
            Vec2_ tangent = Cross(c->normal, 1.0); // 垂直于法向量的切向量
            FLOAT_ rt1 = Dot(c->r1, tangent);     // r1 在切向量上的投影
            kTangent += object1->invMomentInertia * (R1 - rt1 * rt1);
            c->massTangent = 1.0 / kTangent;

            // 计算位置修正值（用于穿透修复）
            c->bias = -k_biasFactor * inv_dt * std::min(FLOAT_(0.0), c->separation + k_allowedPenetration);

            // 应用累积的冲量（地图是静态的，只更新 object1）
            Vec2_ P = c->Pn * c->normal - c->Pt * tangent;
            object1->speed -= object1->invMass * P;
            object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, P);
        }
    }

    /**
     * @brief   应用冲量
     * @details 根据碰撞信息计算并应用法向冲量和切向冲量（摩擦力）
     *          地图是静态的，所以只更新 object1 的速度和角速度
     */
    void PhysicsBaseArbiterA::ApplyImpulse()
    {
        // 如果 object1 质量无穷大（固定物体），无需处理
        if (object1->invMass == 0)
            return;

        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;

            // 计算接触点的相对速度（地图静止，相对速度 = -object1 的速度）
            Vec2_ dv = -object1->speed - Cross(object1->angleSpeed, c->r1);

            // 计算法向冲量
            FLOAT_ vn = Dot(dv, c->normal);          // 相对速度在法向的分量
            FLOAT_ dPn = c->massNormal * (-vn + c->bias); // 需要施加的法向冲量

            // 夹紧累积冲量（防止负冲量）
            FLOAT_ Pn0 = c->Pn;
            c->Pn = std::max(Pn0 + dPn, FLOAT_(0.0));
            dPn = c->Pn - Pn0;

            // 应用法向冲量（只更新 object1）
            Vec2_ Pn = dPn * c->normal;
            object1->speed -= object1->invMass * Pn;
            object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, Pn);

            // 重新计算相对速度（法向冲量已改变速度）
            dv = -object1->speed - Cross(object1->angleSpeed, c->r1);

            // 计算切向冲量（摩擦力）
            Vec2_ tangent = Cross(c->normal, -1.0);  // 切向量方向与法向量垂直
            FLOAT_ vt = Dot(dv, tangent);            // 相对速度在切向的分量
            FLOAT_ dPt = c->massTangent * (-vt);     // 需要施加的切向冲量

            // 计算最大摩擦力（受法向力限制）
            FLOAT_ maxPt = c->friction * c->Pn;

            // 夹紧切向冲量
            FLOAT_ oldTangentImpulse = c->Pt;
            c->Pt = Clamp(oldTangentImpulse + dPt, -maxPt, maxPt);
            dPt = c->Pt - oldTangentImpulse;

            // 应用切向冲量（只更新 object1）
            Vec2_ Pt = dPt * tangent;
            object1->speed -= object1->invMass * Pt;
            object1->angleSpeed -= object1->invMomentInertia * Cross(c->r1, Pt);
        }
    }

    // ========== PhysicsBaseArbiterD 实现（动态点 + 地图）==========

    /**
     * @brief   更新碰撞信息
     * @param   NewContacts 新的碰撞点数组
     * @param   numNewContacts 新碰撞点数量
     * @details 将新碰撞信息复制到内部，并保留之前帧的冲量累积值（Pn和Pt）
     *          点对象没有旋转，地图是静态的，只涉及线速度计算
     */
    void PhysicsBaseArbiterD::Update(Contact *NewContacts, int numNewContacts)
    {
        // 保留之前的冲量累积值（用于帧间冲量缓存）
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
        // 更新碰撞点信息（法向量、位置、分离距离、接触面标识、冲量累积）
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

    /**
     * @brief   预处理（无时间步长版本）
     * @details 计算碰撞点相对于点对象位置的向量
     *          点对象没有旋转，地图是静态的，所以 r2 始终为零向量
     */
    void PhysicsBaseArbiterD::PreStep()
    {
        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos; // 点位置到碰撞点的向量
            c->r2 = {0, 0};                     // 地图是静态的，没有质心偏移
        }
    }

    /**
     * @brief   预处理（带时间步长版本）
     * @param   inv_dt 时间步长的倒数
     * @details 计算有效质量、位置修正值，并应用累积冲量
     *          点对象没有旋转，地图是静态的，所以只计算线速度相关的有效质量
     */
    void PhysicsBaseArbiterD::PreStep(FLOAT_ inv_dt)
    {
        // 如果点对象质量无穷大（固定点），无需处理
        if (object1->invMass == 0)
            return;

        const FLOAT_ k_allowedPenetration = 0.01;    // 允许穿透距离
        const FLOAT_ k_biasFactor = k_biasFactorVAL; // 位置修正系数

        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->pos;
            c->r2 = {0, 0};

            // 计算法向有效质量（点没有旋转，地图质量无穷大）
            FLOAT_ rn1 = Dot(c->r1, c->normal);
            FLOAT_ R1 = Dot(c->r1, c->r1);
            FLOAT_ kNormal = object1->invMass;
            c->massNormal = 1.0 / kNormal;

            // 计算切向有效质量（点没有旋转，切向质量等于法向质量）
            Vec2_ tangent = Cross(c->normal, 1.0);
            c->massTangent = 1.0 / kNormal;

            // 计算位置修正值（用于穿透修复）
            c->bias = -k_biasFactor * inv_dt * std::min(FLOAT_(0.0), c->separation + k_allowedPenetration);

            // 应用累积的冲量（只更新点对象的线速度）
            Vec2_ P = c->Pn * c->normal + c->Pt * tangent;
            object1->speed -= object1->invMass * P;
        }
    }

    /**
     * @brief   应用冲量
     * @details 根据碰撞信息计算并应用法向冲量和切向冲量（摩擦力）
     *          点对象没有旋转，地图是静态的，所以只更新点对象的线速度
     */
    void PhysicsBaseArbiterD::ApplyImpulse()
    {
        // 如果点对象质量无穷大（固定点），无需处理
        if (object1->invMass == 0)
            return;

        Contact *c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;

            // 计算相对速度（地图静止，相对速度 = -点对象的速度）
            Vec2_ dv = -object1->speed;

            // 计算法向冲量
            FLOAT_ vn = Dot(dv, c->normal);          // 相对速度在法向的分量
            FLOAT_ dPn = c->massNormal * (-vn + c->bias); // 需要施加的法向冲量

            // 夹紧累积冲量（防止负冲量）
            FLOAT_ Pn0 = c->Pn;
            c->Pn = std::max(Pn0 + dPn, FLOAT_(0.0));
            dPn = c->Pn - Pn0;

            // 应用法向冲量（只更新点对象的线速度）
            Vec2_ Pn = dPn * c->normal;
            object1->speed -= object1->invMass * Pn;

            // 重新计算相对速度（法向冲量已改变速度）
            dv = -object1->speed;

            // 计算切向冲量（摩擦力）
            Vec2_ tangent = Cross(c->normal, 1.0);   // 切向量方向与法向量垂直
            FLOAT_ vt = Dot(dv, tangent);            // 相对速度在切向的分量
            FLOAT_ dPt = c->massTangent * (-vt);     // 需要施加的切向冲量

            // 计算最大摩擦力（受法向力限制）
            FLOAT_ maxPt = c->friction * c->Pn;

            // 夹紧切向冲量
            FLOAT_ oldTangentImpulse = c->Pt;
            c->Pt = Clamp(oldTangentImpulse + dPt, -maxPt, maxPt);
            dPt = c->Pt - oldTangentImpulse;

            // 应用切向冲量（只更新点对象的线速度）
            Vec2_ Pt = dPt * tangent;
            object1->speed -= object1->invMass * Pt;
        }
    }

}
