#include "PhysicsCircle3D.hpp"

namespace PhysicsBlock3D
{

	/**
	 * @brief 构造函数
	 * @param Pos 球的位置
	 * @param Radius 球的半径
	 * @param Mass 球的质量
	 * @param Friction 摩擦因数
	 * @details 1. 调用 PhysicsAngle3D 构造函数初始化位置、质量、摩擦因数和半径
	 *          2. 根据质量计算转动惯量张量
	 *          3. 如果质量为 FLOAT_MAX（不可移动），则设惯量为 FLOAT_MAX、逆为 0
	 *          4. 否则用实心球公式 I = (2/5)·m·r² 计算，各轴相同（各向同性）
	 *
	 * @note 2D 圆盘是 I = ½mr²（绕 Z 轴）；3D 实心球是 I = (2/5)mr²。
	 *       球体各向同性，故张量为对角阵且三个对角元相等。
	 */
	PhysicsCircle3D::PhysicsCircle3D(Vec3_ Pos, FLOAT_ Radius, FLOAT_ Mass, FLOAT_ Friction)
	    : PhysicsAngle3D(Pos, Mass, Friction, Radius)
	{
		type = PhysicsObjectEnum::circle;
		if (mass == FLOAT_MAX)
		{
			MomentInertia = Mat3_(FLOAT_MAX);
			invMomentInertia = Mat3_(0.0f);
		}
		else
		{
			const FLOAT_ I = FLOAT_(0.4) * mass * radius * radius; // 2/5
			MomentInertia = Mat3_(I);
			if (I <= FLOAT_(0))
			{
				invMomentInertia = Mat3_(0.0f);
			}
			else
			{
				invMomentInertia = Mat3_(FLOAT_(1.0) / I);
			}
		}
	}

	/**
	 * @brief 析构函数
	 */
	PhysicsCircle3D::~PhysicsCircle3D()
	{
	}

}
