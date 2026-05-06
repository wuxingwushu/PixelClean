#include "PhysicsLine.hpp"

namespace PhysicsBlock
{

    /** @brief 构造物理线段
     *  @details 初始化线段的中心位置、半径、角度、转动惯量等物理属性
     *  @param begin_ 线段起点
     *  @param end_ 线段终点
     *  @param mass_ 质量
     *  @param friction_ 摩擦系数 */
    PhysicsLine::PhysicsLine(Vec2_ begin_, Vec2_ end_, FLOAT_ mass_, FLOAT_ friction_) : PhysicsAngle((begin_ + end_) / FLOAT_(2), mass_, friction_) {
        type = PhysicsObjectEnum::line;
        radius = Modulus(begin_ - end_);
        angle = EdgeVecToCosAngleFloat(begin_ - end_);
        MomentInertia = radius * radius * mass_ / 12;
        if (MomentInertia <= FLOAT_(0))
        {
            invMomentInertia = 0;
        }
        else
        {
            invMomentInertia = FLOAT_(1.0) / MomentInertia;
        }
        radius /= 2;
    }

}