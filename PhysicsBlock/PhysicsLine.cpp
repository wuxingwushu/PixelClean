#include "PhysicsLine.hpp"

namespace PhysicsBlock
{

    PhysicsLine::PhysicsLine(Vec2_ begin_, Vec2_ end_, FLOAT_ mass_, FLOAT_ friction_) : PhysicsAngle((begin_ + end_) / FLOAT_(2), mass_, friction_) {
        radius = Modulus(begin_ - end_);
        angle = EdgeVecToCosAngleFloat(begin_ - end_);
        MomentInertia = radius * radius * mass_ / 12;
        invMomentInertia = 1.0 / MomentInertia;
        radius /= 2;
    }

}