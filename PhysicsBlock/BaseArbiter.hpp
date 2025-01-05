#pragma once
#include <glm/glm.hpp>
#include "PhysicsFormwork.hpp"

namespace PhysicsBlock
{
    struct Contact
    {
        Contact() : Pn(0.0), Pt(0.0) /*, Pnb(0.0)*/ {}

        // Collide
        glm::dvec2 position; // 碰撞点位置
        glm::dvec2 normal;   // 最佳分离轴 的 法向量
        double separation;   // 最小分离距离

        // ApplyImpulse
        glm::dvec2 r1; // A物体质心 指向 碰撞点 的向量
        glm::dvec2 r2; // B物体质心 指向 碰撞点 的向量
        double Pn;     // 位移影响量大小
        double Pt;     // 旋转影响量大小
        // double Pnb; // accumulated normal impulse for position bias

        // PreStep
        double massNormal;  // 计算 位移 冲量的分母
        double massTangent; // 计算 旋转 冲量的分母
        double bias;        // 物体位置修正值大小？
    };

    struct ArbiterKey
    {
        ArbiterKey(PhysicsFormwork *Object1, PhysicsFormwork *Object2)
        {
            if (Object1 < Object2)
            {
                object1 = Object1;
                object2 = Object2;
            }
            else
            {
                object1 = Object2;
                object2 = Object1;
            }
        }
        // 碰撞的两个对象
        PhysicsFormwork *object1;
        PhysicsFormwork *object2;
    };

    inline bool operator<(const ArbiterKey &a1, const ArbiterKey &a2)
    {
    	if (a1.object1 < a2.object1)
    		return true;

    	if (a1.object1 == a2.object1 && a1.object2 < a2.object2)
    		return true;

    	return false;
    }


    class BaseArbiter: public ArbiterKey
    { 
    public:
        Contact contacts[2]; // 碰撞点集合
        int numContacts;     // 碰撞点数量
        double friction;     // 两个物体间的摩擦系数
    public:
        BaseArbiter(PhysicsFormwork *Object1, PhysicsFormwork *Object2):
            ArbiterKey(Object1, Object2){};
        ~BaseArbiter(){};

        // 更新碰撞信息
        virtual void Update(Contact *NewContacts, int numNewContacts) = 0;
        // 预处理
        virtual void PreStep(float inv_dt) = 0;
        // 迭代出结果
        virtual void ApplyImpulse() = 0;
    };
    
}