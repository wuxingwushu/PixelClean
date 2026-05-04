#include "PhysicsCircle.hpp"

namespace PhysicsBlock
{

    /**
     * @brief 构造函数
     * @param Pos 圆形的位置
     * @param Radius 圆形的半径
     * @param Mass 圆形的质量
     * @param Friction 摩擦因数
     * @details 1. 调用 PhysicsAngle 构造函数初始化位置、质量、摩擦因数和半径
     * 2. 根据质量计算转动惯量
     * 3. 如果质量为 FLOAT_MAX（不可移动），则设置转动惯量为 FLOAT_MAX，转动惯量倒数为 0
     * 4. 否则，使用公式 I = 0.5 * m * r^2 计算转动惯量，并计算转动惯量倒数
     */
    PhysicsCircle::PhysicsCircle(Vec2_ Pos, FLOAT_ Radius, FLOAT_ Mass, FLOAT_ Friction)
    : PhysicsAngle(Pos, Mass, Friction, Radius)
    {
        if (mass == FLOAT_MAX) {
            MomentInertia = FLOAT_MAX;
            invMomentInertia = 0;
        }else{
            MomentInertia = FLOAT_(0.5) * mass * radius * radius;
            if (MomentInertia <= FLOAT_(0))
            {
                invMomentInertia = 0;
            }
            else
            {
                invMomentInertia = FLOAT_(1.0) / MomentInertia;
            }
        }
    }
    
    /**
     * @brief 析构函数
     */
    PhysicsCircle::~PhysicsCircle()
    {
    }

}
