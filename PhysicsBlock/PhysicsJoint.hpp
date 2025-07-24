#pragma once
#include "PhysicsShape.hpp"

namespace PhysicsBlock
{

    class PhysicsJoint
    {
    private:
        Mat2_ M;                          // 迭代，力方向
        Vec2_ localAnchor1, localAnchor2; // 物体延伸出来的杠杆
        Vec2_ P;                          // 累计推理
        Vec2_ bias;                       // 偏移量
        FLOAT_ biasFactor;                     // 偏移矫正量
        FLOAT_ softness;                       // 柔软度
    public:
        Vec2_ r1, r2;           // 计算杠杆节点位置（用于判断位置偏移多少）
        PhysicsAngle *body1, *body2; // 对应两个物体
    public:
        PhysicsJoint(/* args */);
        ~PhysicsJoint();

        void Set(PhysicsAngle *Body1, PhysicsAngle *Body2, const Vec2_ anchor);

        void PreStep(FLOAT_ inv_dt);

        void ApplyImpulse();
    };

}
