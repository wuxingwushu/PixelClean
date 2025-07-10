#pragma once
#include "PhysicsShape.hpp"

namespace PhysicsBlock
{

    class PhysicsJoint
    {
    private:
        glm::dmat2 M;                          // 迭代，力方向
        glm::dvec2 localAnchor1, localAnchor2; // 物体延伸出来的杠杆
        glm::dvec2 P;                          // 累计推理
        glm::dvec2 bias;                       // 偏移量
        double biasFactor;                     // 偏移矫正量
        double softness;                       // 柔软度
    public:
        glm::dvec2 r1, r2;           // 计算杠杆节点位置（用于判断位置偏移多少）
        PhysicsShape *body1, *body2; // 对应两个物体
    public:
        PhysicsJoint(/* args */);
        ~PhysicsJoint();

        void Set(PhysicsShape *Body1, PhysicsShape *Body2, const glm::dvec2 anchor);

        void PreStep(double inv_dt);

        void ApplyImpulse();
    };

}
