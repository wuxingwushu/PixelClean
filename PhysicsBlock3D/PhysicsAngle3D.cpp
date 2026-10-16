#include "PhysicsAngle3D.hpp"
#include <cmath>

namespace PhysicsBlock3D
{

	/**
	 * @brief 构造函数（带半径）
	 * @param Pos 位置
	 * @param Mass 质量
	 * @param Friction 摩擦因数
	 * @param Radius 碰撞半径
	 */
	PhysicsAngle3D::PhysicsAngle3D(Vec3_ Pos, FLOAT_ Mass, FLOAT_ Friction, FLOAT_ Radius)
	    : PhysicsParticle3D(Pos, Mass, Friction), radius(Radius)
	{
		type = PhysicsObjectEnum::particle;
	}

	/**
	 * @brief 构造函数（无半径）
	 */
	PhysicsAngle3D::PhysicsAngle3D(Vec3_ Pos, FLOAT_ Mass, FLOAT_ Friction)
	    : PhysicsParticle3D(Pos, Mass, Friction)
	{
		type = PhysicsObjectEnum::particle;
	}

	/**
	 * @brief 构造函数（仅位置）
	 */
	PhysicsAngle3D::PhysicsAngle3D(Vec3_ Pos) : PhysicsParticle3D(Pos)
	{
		type = PhysicsObjectEnum::particle;
	}

	PhysicsAngle3D::~PhysicsAngle3D()
	{
	}

	/**
	 * @brief 受力（移动 + 旋转）
	 * @details 3D 力矩为叉积向量：τ += (Pos - pos) × Force
	 *          （2D 为标量 τ += r.x*F.y - r.y*F.x，即叉积的 z 分量）
	 */
	void PhysicsAngle3D::AddForce(Vec3_ Pos, Vec3_ Force)
	{
		if (invMass == 0)
		{
			return;
		}
		const Vec3_ Arm = Pos - pos;
		PhysicsParticle3D::AddForce(Force);
		torque += Cross(Arm, Force);
	}

	/**
	 * @brief 受力（仅移动）
	 */
	void PhysicsAngle3D::AddForce(Vec3_ Force)
	{
		PhysicsParticle3D::AddForce(Force);
	}

	/**
	 * @brief 施加纯力矩（仅旋转）
	 * @param ArmForce 力矩向量（力臂 × 力）
	 * @note 2D 版是 `AddTorque(FLOAT_ ArmForce, FLOAT_ Force)` 做标量乘法；
	 *       3D 直接接收力矩向量。
	 */
	void PhysicsAngle3D::AddTorque(const Vec3_ &ArmForce)
	{
		if (invMass == 0)
		{
			return;
		}
		torque += ArmForce;
	}

	/**
	 * @brief 施加质心冲量
	 */
	void PhysicsAngle3D::ApplyImpulse(const Vec3_ &impulse)
	{
		if (invMass == 0)
		{
			return;
		}
		speed += invMass * impulse;
		StaticNum = 0;
	}

	/**
	 * @brief 施加带受力点的冲量（同时产生角速度）
	 * @details 线速度 Δv = invMass * impulse
	 *          角速度 Δω = invI_world * (r × impulse)，invI_world 为世界系张量
	 *          （2D 为标量 Δω = invI * (r.x*J.y - r.y*J.x)）
	 */
	void PhysicsAngle3D::ApplyImpulse(const Vec3_ &impulse, const Vec3_ &worldPoint)
	{
		if (invMass == 0)
		{
			return;
		}
		speed += invMass * impulse;
		const Vec3_ r = worldPoint - pos;
		angleSpeed += WorldInvInertia() * Cross(r, impulse);
		StaticNum = 0;
	}

	/**
	 * @brief 施加纯角冲量
	 * @details Δω = invI_world * torqueImpulse（世界系张量乘向量）
	 */
	void PhysicsAngle3D::ApplyTorqueImpulse(const Vec3_ &torqueImpulse)
	{
		if (invMass == 0)
		{
			return;
		}
		angleSpeed += WorldInvInertia() * torqueImpulse;
		StaticNum = 0;
	}

	/**
	 * @brief 根据外力和时间更新速度与角速度
	 */
	void PhysicsAngle3D::PhysicsSpeed(FLOAT_ time, Vec3_ Ga)
	{
		PhysicsParticle3D::PhysicsSpeed(time, Ga);
		if (invMass == 0)
		{
			return;
		}
#if Define_MinSpoilageBool3D
		angleSpeed *= MinSpoilageFactor(time);
#endif
		angleSpeed += time * (WorldInvInertia() * torque);
		torque = Vec3_(0.0f);
	}

	/**
	 * @brief 根据速度和时间更新位置与朝向
	 * @details 朝向积分：rotation = normalize(dq * rotation)，
	 *          其中 dq 由角速度与时间步构造（世界系增量旋转左乘）。
	 *          （2D 为 `angle += time * angleSpeed`）
	 */
	void PhysicsAngle3D::PhysicsPos(FLOAT_ time, Vec3_ Ga)
	{
		PhysicsParticle3D::PhysicsPos(time, Ga);
		if (invMass == 0)
		{
			return;
		}
		const Quat_ dq = QuatFromAngularVelocity(angleSpeed, time);
		rotation = glm::normalize(dq * rotation);
	}

	/**
	 * @brief 取绕 Y 轴的偏航角
	 * @details 把物体局部 +X 轴旋到世界，取其 XZ 平面分量求 atan2。
	 *          仅供需要平面朝向的逻辑（如面向插值）使用。
	 */
	FLOAT_ PhysicsAngle3D::GetYaw() const
	{
		const Vec3_ forward = rotation * Vec3_(1.0f, 0.0f, 0.0f);
		return std::atan2(-forward.z, forward.x);
	}

#if PhysicsBlock_Serialization3D
	/**
	 * @brief 从 JSON 构造
	 */
	PhysicsAngle3D::PhysicsAngle3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
	    : PhysicsParticle3D(Vec3_(0.0f))
	{
		type = PhysicsObjectEnum::particle;
		JsonContrarySerialization(data);
	}

	/**
	 * @brief 序列化
	 */
	void PhysicsAngle3D::JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data)
	{
		PhysicsParticle3D::JsonSerialization(data);
		SerializationMat3(data, MomentInertia);
		SerializationMat3(data, invMomentInertia);
		SerializationQuat(data, rotation);
		SerializationVec3(data, angleSpeed);
		SerializationVec3(data, torque);
		data["radius"] = radius;
	}

	/**
	 * @brief 反序列化
	 * @details 与 2D 一致：若 MomentInertia 的某个对角元为 FLOAT_MAX，
	 *          视作"无限惯量"，逆张量置零。
	 */
	void PhysicsAngle3D::JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
	{
		PhysicsParticle3D::JsonContrarySerialization(data);
		ContrarySerializationMat3(data, MomentInertia);
		ContrarySerializationVec3(data, angleSpeed);
		ContrarySerializationVec3(data, torque);
		ContrarySerializationQuat(data, rotation);
		radius = data["radius"];

		// 无限惯量判定：任一对角元为 FLOAT_MAX 即视为不可旋转
		const bool infinite = (MomentInertia[0][0] == FLOAT_MAX) ||
		                      (MomentInertia[1][1] == FLOAT_MAX) ||
		                      (MomentInertia[2][2] == FLOAT_MAX);
		if (infinite)
		{
			invMomentInertia = Mat3_(0.0f);
		}
		else
		{
			invMomentInertia = SymmetricInverse(MomentInertia);
		}
	}
#endif

}
