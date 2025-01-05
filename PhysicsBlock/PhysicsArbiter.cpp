#include "PhysicsArbiter.hpp"
#include "BaseCalculate.hpp"
#include <utility>

namespace PhysicsBlock
{
    PhysicsArbiterSS::PhysicsArbiterSS(PhysicsFormwork *Object1, PhysicsFormwork *Object2):
        BaseArbiter(Object1, Object2)
    {

    }

    // 更新碰撞信息
    void PhysicsArbiterSS::Update(Contact *NewContacts, int numNewContacts){

    }

    
    // 预处理
    void PhysicsArbiterSS::PreStep(double inv_dt){
        const double k_allowedPenetration = 0.01;// 容許穿透
	    const double k_biasFactor = 0.2;// 位置修正量
        
        // 獲取碰撞點
		Contact* c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->PFGetPos();// object1 质心 指向碰撞点的 力矩
		    c->r2 = c->position - object2->PFGetPos();// object2 质心 指向碰撞点的 力矩
        
            double rn1 = Dot(c->r1, c->normal);// box1质心指向碰撞点 到 法向量 的 投影   
		    double rn2 = Dot(c->r2, c->normal);// box2质心指向碰撞点 到 法向量 的 投影
            double R1 = Dot(c->r1, c->r1);
            double R2 = Dot(c->r2, c->r2);
            double kNormal = object1->PFGetInvMass() + object2->PFGetInvMass();
            double kTangent = kNormal;
            kNormal += object1->PFGetInvI() * (R1 - rn1 * rn1) + object2->PFGetInvI() * (R2 - rn2 * rn2);
		    c->massNormal = 1.0 / kNormal;

            glm::dvec2 tangent = Cross(c->normal, 1.0);// 垂直 normal 的 法向量
		    double rt1 = Dot(c->r1, tangent);// box1质心指向碰撞点 到 垂直法向量 的 投影
		    double rt2 = Dot(c->r2, tangent);// box2质心指向碰撞点 到 垂直法向量 的 投影
		    kTangent += object1->PFGetInvI() * (R1 - rt1 * rt1) + object2->PFGetInvI() * (R2 - rt2 * rt2);
		    c->massTangent = 1.0 / kTangent;

            c->bias = -k_biasFactor * inv_dt * std::min(0.0, c->separation + k_allowedPenetration);// 物体位置修正值大小
        
            // 施加正常+摩擦脉冲
			glm::dvec2 P = c->Pn * c->normal + c->Pt * tangent;

			object1->PFSpeed() -= object1->PFGetInvMass() * P;
			object1->PFAngleSpeed() -= object1->PFGetInvI() * Cross(c->r1, P);

			object2->PFSpeed() -= object2->PFGetInvMass() * P;
			object2->PFAngleSpeed() += object2->PFGetInvI() * Cross(c->r2, P);
        }
        
    }

    // 迭代出结果
    void PhysicsArbiterSS::ApplyImpulse(){
        Contact* c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;

            // 接触时的相对速度
            glm::dvec2 dv = object2->PFSpeed() + Cross(object2->PFAngleSpeed(), c->r2) - object1->PFSpeed() - Cross(object1->PFAngleSpeed(), c->r1);
        
            // 计算法向脉冲
		    double vn = Dot(dv, c->normal);// 作用于对方的速度

		    double dPn = c->massNormal * (-vn + c->bias);// 移动速度大小修补值

            // 夹紧累积的脉冲
			double Pn0 = c->Pn;
			c->Pn = std::max(Pn0 + dPn, 0.0);
			dPn = c->Pn - Pn0;

            // 应用接触脉冲
		    glm::dvec2 Pn = dPn * c->normal;

		    object1->PFSpeed() -= object1->PFGetInvMass() * Pn;
		    object1->PFAngleSpeed() -= object1->PFGetInvI() * Cross(c->r1, Pn);

		    object2->PFSpeed() += object2->PFGetInvMass() * Pn;
		    object2->PFAngleSpeed() += object2->PFGetInvI() * Cross(c->r2, Pn);






            // 接触时的相对速度
            dv = object2->PFSpeed() + Cross(object2->PFAngleSpeed(), c->r2) - object1->PFSpeed() - Cross(object1->PFAngleSpeed(), c->r1);
        
            glm::dvec2 tangent = Cross(c->normal, 1.0);
		    double vt = Dot(dv, tangent);// 作用于对方的角速度
		    double dPt = c->massTangent * (-vt);// 旋转速度大小修补值

            // 计算摩擦脉冲
			double maxPt = friction * c->Pn;

			// 夹具摩擦
			double oldTangentImpulse = c->Pt;
			c->Pt = Clamp(oldTangentImpulse + dPt, -maxPt, maxPt);
			dPt = c->Pt - oldTangentImpulse;

            // 应用接触脉冲
		    glm::dvec2 Pt = dPt * c->normal;

		    object1->PFSpeed() -= object1->PFGetInvMass() * Pt;
		    object1->PFAngleSpeed() -= object1->PFGetInvI() * Cross(c->r1, Pt);

		    object2->PFSpeed() += object2->PFGetInvMass() * Pt;
		    object2->PFAngleSpeed() += object2->PFGetInvI() * Cross(c->r2, Pt);
        }
    }








    PhysicsArbiterSP::PhysicsArbiterSP(PhysicsFormwork *Object1, PhysicsFormwork *Object2):
        BaseArbiter(Object1, Object2)
    {
        
    }

    // 更新碰撞信息
    void PhysicsArbiterSP::Update(Contact *NewContacts, int numNewContacts){
        
    }

    
    // 预处理
    void PhysicsArbiterSP::PreStep(double inv_dt){
        const double k_allowedPenetration = 0.01;// 容許穿透
	    const double k_biasFactor = 0.2;// 位置修正量
        
        // 獲取碰撞點
		Contact* c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->PFGetPos();// object1 质心 指向碰撞点的 力矩
		    //c->r2 = c->position - object2->PFGetPos();// object2 质心 指向碰撞点的 力矩
        
            double rn1 = Dot(c->r1, c->normal);// box1质心指向碰撞点 到 法向量 的 投影   
            double R1 = Dot(c->r1, c->r1);
            double kNormal = object1->PFGetInvMass() + object2->PFGetInvMass();
            double kTangent = kNormal;
            kNormal += object1->PFGetInvI() * (R1 - rn1 * rn1);
		    c->massNormal = 1.0 / kNormal;

            glm::dvec2 tangent = Cross(c->normal, 1.0);// 垂直 normal 的 法向量
		    double rt1 = Dot(c->r1, tangent);// box1质心指向碰撞点 到 垂直法向量 的 投影
		    kTangent += object1->PFGetInvI() * (R1 - rt1 * rt1);
		    c->massTangent = 1.0f / kTangent;

            c->bias = -k_biasFactor * inv_dt * std::min(0.0, c->separation + k_allowedPenetration);// 物体位置修正值大小
        
            // 施加正常+摩擦脉冲
			glm::dvec2 P = c->Pn * c->normal + c->Pt * tangent;

			object1->PFSpeed() -= object1->PFGetInvMass() * P;
			object1->PFAngleSpeed() -= object1->PFGetInvI() * Cross(c->r1, P);
            //object1->PFAngleSpeed() -= (object1->PFGetInvI() * Cross(c->r1, P) + object2->PFGetInvI() * Cross(c->r2, P));

			object2->PFSpeed() -= object2->PFGetInvMass() * P;
			//object2->PFAngleSpeed() += object2->PFGetInvI() * Cross(c->r2, P);
        }
        
    }

    // 迭代出结果
    void PhysicsArbiterSP::ApplyImpulse(){
        Contact* c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;

            // 接触时的相对速度
            glm::dvec2 dv = object2->PFSpeed() - object1->PFSpeed() - Cross(object1->PFAngleSpeed(), c->r1);
        
            // 计算法向脉冲
		    double vn = Dot(dv, c->normal);// 作用于对方的速度

		    double dPn = c->massNormal * (-vn + c->bias);// 移动速度大小修补值

            // 夹紧累积的脉冲
			double Pn0 = c->Pn;
			c->Pn = std::max(Pn0 + dPn, 0.0);
			dPn = c->Pn - Pn0;

            // 应用接触脉冲
		    glm::dvec2 Pn = dPn * c->normal;

		    object1->PFSpeed() -= object1->PFGetInvMass() * Pn;
		    object1->PFAngleSpeed() -= object1->PFGetInvI() * Cross(c->r1, Pn);

		    object2->PFSpeed() += object2->PFGetInvMass() * Pn;
		    //object2->PFAngleSpeed() += object2->PFGetInvI() * Cross(c->r2, Pn);






            // 接触时的相对速度
            dv = object2->PFSpeed() - object1->PFSpeed() - Cross(object1->PFAngleSpeed(), c->r1);
        
            glm::dvec2 tangent = Cross(c->normal, 1.0);
		    double vt = Dot(dv, tangent);// 作用于对方的角速度
		    double dPt = c->massTangent * (-vt);// 旋转速度大小修补值

            // 计算摩擦脉冲
			double maxPt = friction * c->Pn;

			// 夹具摩擦
			double oldTangentImpulse = c->Pt;
			c->Pt = Clamp(oldTangentImpulse + dPt, -maxPt, maxPt);
			dPt = c->Pt - oldTangentImpulse;

            // 应用接触脉冲
		    glm::dvec2 Pt = dPt * c->normal;

		    object1->PFSpeed() -= object1->PFGetInvMass() * Pt;
		    object1->PFAngleSpeed() -= object1->PFGetInvI() * Cross(c->r1, Pt);

		    object2->PFSpeed() += object2->PFGetInvMass() * Pt;
		    //object2->PFAngleSpeed() += object2->PFGetInvI() * Cross(c->r2, Pt);
        }
    }








    PhysicsArbiterS::PhysicsArbiterS(PhysicsFormwork *Object1, PhysicsFormwork *Object2):
        BaseArbiter(Object1, Object2)
    {

    }

    // 更新碰撞信息
    void PhysicsArbiterS::Update(Contact *NewContacts, int numNewContacts){

    }

    
    // 预处理
    void PhysicsArbiterS::PreStep(double inv_dt){
        const double k_allowedPenetration = 0.01;// 容許穿透
	    const double k_biasFactor = 0.2;// 位置修正量
        
        // 獲取碰撞點
		Contact* c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->PFGetPos();// object1 质心 指向碰撞点的 力矩
        
            double rn1 = Dot(c->r1, c->normal);// box1质心指向碰撞点 到 法向量 的 投影   
            double R1 = Dot(c->r1, c->r1);
            double kNormal = object1->PFGetInvMass();
            double kTangent = kNormal;
            kNormal += object1->PFGetInvI() * (R1 - rn1 * rn1);
		    c->massNormal = 1.0 / kNormal;

            glm::dvec2 tangent = Cross(c->normal, 1.0);// 垂直 normal 的 法向量
		    double rt1 = Dot(c->r1, tangent);// box1质心指向碰撞点 到 垂直法向量 的 投影
		    kTangent += object1->PFGetInvI() * (R1 - rt1 * rt1);
		    c->massTangent = 1.0 / kTangent;

            c->bias = -k_biasFactor * inv_dt * std::min(0.0, c->separation + k_allowedPenetration);// 物体位置修正值大小
        
            // 施加正常+摩擦脉冲
			glm::dvec2 P = c->Pn * c->normal + c->Pt * tangent;

			object1->PFSpeed() -= object1->PFGetInvMass() * P;
			object1->PFAngleSpeed() -= object1->PFGetInvI() * Cross(c->r1, P);
        }
        
    }

    // 迭代出结果
    void PhysicsArbiterS::ApplyImpulse(){
        Contact* c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;

            // 接触时的相对速度
            glm::dvec2 dv = - object1->PFSpeed() - Cross(object1->PFAngleSpeed(), c->r1);
        
            // 计算法向脉冲
		    double vn = Dot(dv, c->normal);// 作用于对方的速度

		    double dPn = c->massNormal * (-vn + c->bias);// 移动速度大小修补值

            // 夹紧累积的脉冲
			double Pn0 = c->Pn;
			c->Pn = std::max(Pn0 + dPn, 0.0);
			dPn = c->Pn - Pn0;

            // 应用接触脉冲
		    glm::dvec2 Pn = dPn * c->normal;

		    object1->PFSpeed() -= object1->PFGetInvMass() * Pn;
		    object1->PFAngleSpeed() -= object1->PFGetInvI() * Cross(c->r1, Pn);





            // 接触时的相对速度
            dv =  - object1->PFSpeed() - Cross(object1->PFAngleSpeed(), c->r1);
        
            glm::dvec2 tangent = Cross(c->normal, 1.0);
		    double vt = Dot(dv, tangent);// 作用于对方的角速度
		    double dPt = c->massTangent * (-vt);// 旋转速度大小修补值

            // 计算摩擦脉冲
			double maxPt = friction * c->Pn;

			// 夹具摩擦
			double oldTangentImpulse = c->Pt;
			c->Pt = Clamp(oldTangentImpulse + dPt, -maxPt, maxPt);
			dPt = c->Pt - oldTangentImpulse;

            // 应用接触脉冲
		    glm::dvec2 Pt = dPt * c->normal;

		    object1->PFSpeed() -= object1->PFGetInvMass() * Pt;
		    object1->PFAngleSpeed() -= object1->PFGetInvI() * Cross(c->r1, Pt);
        }
    }









    PhysicsArbiterP::PhysicsArbiterP(PhysicsFormwork *Object1, PhysicsFormwork *Object2):
        BaseArbiter(Object1, Object2)
    {

    }

    // 更新碰撞信息
    void PhysicsArbiterP::Update(Contact *NewContacts, int numNewContacts){

    }

    
    // 预处理
    void PhysicsArbiterP::PreStep(double inv_dt){
        const double k_allowedPenetration = 0.01;// 容許穿透
	    const double k_biasFactor = 0.2;// 位置修正量
        
        // 獲取碰撞點
		Contact* c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;
            c->r1 = c->position - object1->PFGetPos();// object1 质心 指向碰撞点的 力矩
        
            double rn1 = Dot(c->r1, c->normal);// box1质心指向碰撞点 到 法向量 的 投影   
            double R1 = Dot(c->r1, c->r1);
            double kNormal = object1->PFGetInvMass();
            double kTangent = kNormal;
            kNormal += object1->PFGetInvI() * (R1 - rn1 * rn1);
		    c->massNormal = 1.0 / kNormal;

            glm::dvec2 tangent = Cross(c->normal, 1.0);// 垂直 normal 的 法向量
		    double rt1 = Dot(c->r1, tangent);// box1质心指向碰撞点 到 垂直法向量 的 投影
		    kTangent += object1->PFGetInvI() * (R1 - rt1 * rt1);
		    c->massTangent = 1.0 / kTangent;

            c->bias = -k_biasFactor * inv_dt * std::min(0.0, c->separation + k_allowedPenetration);// 物体位置修正值大小
        
            // 施加正常+摩擦脉冲
			glm::dvec2 P = c->Pn * c->normal + c->Pt * tangent;

			object1->PFSpeed() -= object1->PFGetInvMass() * P;
        }
        
    }

    // 迭代出结果
    void PhysicsArbiterP::ApplyImpulse(){
        Contact* c;
        for (size_t i = 0; i < numContacts; ++i)
        {
            c = contacts + i;

            // 接触时的相对速度
            glm::dvec2 dv = - object1->PFSpeed() - Cross(object1->PFAngleSpeed(), c->r1);
        
            // 计算法向脉冲
		    double vn = Dot(dv, c->normal);// 作用于对方的速度

		    double dPn = c->massNormal * (-vn + c->bias);// 移动速度大小修补值

            // 夹紧累积的脉冲
			double Pn0 = c->Pn;
			c->Pn = std::max(Pn0 + dPn, 0.0);
			dPn = c->Pn - Pn0;

            // 应用接触脉冲
		    glm::dvec2 Pn = dPn * c->normal;

		    object1->PFSpeed() -= object1->PFGetInvMass() * Pn;






            // 接触时的相对速度
            dv = - object1->PFSpeed() + Cross(object1->PFAngleSpeed(), c->r1);
        
            glm::dvec2 tangent = Cross(c->normal, 1.0);
		    double vt = Dot(dv, tangent);// 作用于对方的角速度
		    double dPt = c->massTangent * (-vt);// 旋转速度大小修补值

            // 计算摩擦脉冲
			double maxPt = friction * c->Pn;

			// 夹具摩擦
			double oldTangentImpulse = c->Pt;
			c->Pt = Clamp(oldTangentImpulse + dPt, -maxPt, maxPt);
			dPt = c->Pt - oldTangentImpulse;

            // 应用接触脉冲
		    glm::dvec2 Pt = dPt * c->normal;

		    object1->PFSpeed() -= object1->PFGetInvMass() * Pt;
        }
    }

}