#pragma once
#include "BaseStruct.hpp"
#include <functional>

namespace PhysicsBlock
{
    struct Contact
    {
        Contact() : Pn(0.0), Pt(0.0) /*, Pnb(0.0)*/ {}

        // Collide
        Vec2_ position;      // 碰撞点位置
        Vec2_ normal;        // 最佳分离轴 的 法向量
        FLOAT_ separation;        // 最小分离距离
        FLOAT_ friction = 0.2; // 两个物体间的摩擦系数
        unsigned char w_side = 0; // 小矩形那个边的分离（判别是否是一种力用的）

        // PreStep
        FLOAT_ massNormal;  // 计算 位移 冲量的分母
        FLOAT_ massTangent; // 计算 旋转 冲量的分母
        FLOAT_ bias;        // 物体位置修正值大小？

        // ApplyImpulse
        Vec2_ r1; // A物体质心 指向 碰撞点 的向量
        Vec2_ r2; // B物体质心 指向 碰撞点 的向量
        FLOAT_ Pn = 0; // 位移影响量大小
        FLOAT_ Pt = 0; // 旋转影响量大小
        // FLOAT_ Pnb; // accumulated normal impulse for position bias
    };

    struct ArbiterKey
    {
        ArbiterKey(void *Object1, void *Object2)
        {
            if (Object1 > Object2) {
                object1 = Object1;
                object2 = Object2;
            }
            else {
                object1 = Object2;
                object2 = Object1;
            }
        }
        // 碰撞的两个对象
        void *object1;
        void *object2;
        char PoolID;

        inline bool operator<(const ArbiterKey &a1) const
        {
            if (a1.object1 < object1)
                return true;

            if (a1.object1 == object1 && a1.object2 < object2)
                return true;

            return false;
        }

        inline bool operator==(const ArbiterKey &a1) const
        {
            return (a1.object1 == object1) && (a1.object2 == object2);
        }
    };

    struct ArbiterKeyHash
    {
        inline std::size_t operator()(const ArbiterKey &key) const
        {
            auto hash_combine = [](std::size_t a, std::size_t b) {
                return a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2));
            };

            std::size_t h1 = std::hash<void*>{}(key.object1);
            std::size_t h2 = std::hash<void*>{}(key.object2);
            return hash_combine(h1, h2);
        }
    };

    /**
     * @brief 基础物理裁决 */
    class BaseArbiter
    {
    public:
        Contact contacts[20];  // 碰撞点集合
        int numContacts = 0;       // 碰撞点数量

        ArbiterKey key; // 两个对象的 钥匙键

    public:
        BaseArbiter(void *Object1, void *Object2) : key(Object1, Object2) {};
        ~BaseArbiter() {};

        // 计算两个物体的碰撞点
        virtual void ComputeCollide() = 0;

        // 更新碰撞信息
        virtual void Update(Contact *NewContacts, int numNewContacts) = 0;
        // 预处理
        virtual void PreStep(FLOAT_ inv_dt) = 0;
        virtual void PreStep() = 0;
        // 迭代出结果
        virtual void ApplyImpulse() = 0;
    };

}