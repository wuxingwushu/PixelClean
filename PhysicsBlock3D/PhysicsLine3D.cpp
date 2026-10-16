#include "PhysicsLine3D.hpp"

namespace PhysicsBlock3D
{

	/**
	 * @brief 构造物理线段（胶囊）
	 * @details 初始化线段的中心位置、半长度、朝向、转动惯量张量等物理属性
	 * @param begin_ 线段起点（世界坐标）
	 * @param end_ 线段终点（世界坐标）
	 * @param mass_ 质量
	 * @param friction_ 摩擦系数
	 * @param thickness_ 胶囊厚度（横截面半径）
	 *
	 * @note 朝向：2D 用标量角 `angle = EdgeVecToCosAngleFloat(begin_-end_)`。
	 *       3D 改为四元数——把局部 X 轴旋到线段方向。用
	 *       `glm::rotation` 需要构造正交基，这里直接求"把 (1,0,0) 转到 dir"的
	 *       最短弧四元数：axis = normalize(cross((1,0,0), dir))，
	 *       半角 = acos(dot((1,0,0), dir))。当 dir 与 (1,0,0) 反向（夹角 π）时
	 *       叉积为零，需退化为绕任意垂直轴转 π。
	 */
	PhysicsLine3D::PhysicsLine3D(Vec3_ begin_, Vec3_ end_, FLOAT_ mass_, FLOAT_ friction_,
	                             FLOAT_ thickness_)
	    : PhysicsAngle3D((begin_ + end_) / FLOAT_(2), mass_, friction_)
	{
		type = PhysicsObjectEnum::line;

		const Vec3_ edge = begin_ - end_;
		const FLOAT_ length = Modulus(edge); // 全长

		HalfLength = length * FLOAT_(0.5);
		Thickness = thickness_;

		// radius 保持 2D 语义（半长度），供 PFGetCollisionR 做宽相剔除
		radius = HalfLength;

		// 朝向：把局部 X 轴旋到线段方向
		const Vec3_ dir = SafeNormalize(end_ - begin_);
		const Vec3_ xAxis(FLOAT_(1.0), FLOAT_(0.0), FLOAT_(0.0));
		const FLOAT_ dotVal = Clamp(Dot(xAxis, dir), FLOAT_(-1.0), FLOAT_(1.0));
		const Vec3_ crossVal = Cross(xAxis, dir);
		const FLOAT_ sinVal = Modulus(crossVal);
		if (sinVal > FLOAT_(1e-6))
		{
			// 常规情形：绕叉积轴转 acos(dot)
			const Vec3_ axis = crossVal / sinVal;
			rotation = glm::angleAxis(glm::acos(dotVal), axis);
		}
		else if (dotVal > FLOAT_(0.0))
		{
			// 已同向
			rotation = IdentityQuat();
		}
		else
		{
			// 反向（π）：绕任意垂直轴转 π
			rotation = glm::angleAxis(FLOAT_(3.14159265358979323846), Vec3_(FLOAT_(0.0), FLOAT_(1.0), FLOAT_(0.0)));
		}

		// 胶囊转动惯量（绕质心，轴向为局部 X）：
		//   Ixx = ½mr²
		//   Iyy = Izz = (1/12)m(3r² + L²)
		// 其中 r = Thickness，L = 2*HalfLength（全长）
		const FLOAT_ r2 = Thickness * Thickness;
		const FLOAT_ L2 = length * length;
		const FLOAT_ Ixx = FLOAT_(0.5) * mass_ * r2;
		const FLOAT_ Iyz = (FLOAT_(1.0) / FLOAT_(12.0)) * mass_ * (FLOAT_(3.0) * r2 + L2);

		MomentInertia = Mat3_(0.0f);
		MomentInertia[0][0] = Ixx;
		MomentInertia[1][1] = Iyz;
		MomentInertia[2][2] = Iyz;

		if (mass_ == FLOAT_MAX)
		{
			MomentInertia = Mat3_(FLOAT_MAX);
			invMomentInertia = Mat3_(0.0f);
		}
		else
		{
			// 逐对角元求逆（对角阵，且胶囊公式保证三个对角元都 > 0）
			invMomentInertia = Mat3_(0.0f);
			if (Ixx > FLOAT_(0))
			{
				invMomentInertia[0][0] = FLOAT_(1.0) / Ixx;
			}
			if (Iyz > FLOAT_(0))
			{
				invMomentInertia[1][1] = FLOAT_(1.0) / Iyz;
				invMomentInertia[2][2] = FLOAT_(1.0) / Iyz;
			}
		}
	}

	/**
	 * @brief 取线段的两个端点（世界坐标）
	 * @param outBegin 起点
	 * @param outEnd 终点
	 * @details 局部 X 轴按当前朝向旋转后，从质心两侧各偏移 HalfLength。
	 *          2D 用 `vec2angle({radius,0}, angle)` 取半向量，这里用四元数旋转。
	 */
	void PhysicsLine3D::GetSegment(Vec3_ &outBegin, Vec3_ &outEnd) const
	{
		const Vec3_ halfAxis = QuatRotate(rotation, Vec3_(HalfLength, FLOAT_(0.0), FLOAT_(0.0)));
		outBegin = pos - halfAxis;
		outEnd = pos + halfAxis;
	}

}
