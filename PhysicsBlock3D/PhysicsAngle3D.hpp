#pragma once
#include "PhysicsParticle3D.hpp"
#include "BaseCalculate3D.hpp"

namespace PhysicsBlock3D
{

	/**
	 * @brief 物理角度类（3D）
	 * @details 继承自 PhysicsParticle3D，添加了朝向、角速度、转动惯量张量和扭矩等属性
	 * 用于处理具有旋转能力的物理对象
	 *
	 * @note 由 2D 版 PhysicsAngle 改写。**这是 3D 移植中语义变化最大的一层**：
	 *  - `FLOAT_ angle`        → `Quat_ rotation`（朝向用四元数，避免万向节死锁）
	 *  - `FLOAT_ angleSpeed`   → `Vec3_ angleSpeed`（角速度向量，方向=旋转轴，模长=角速率）
	 *  - `FLOAT_ MomentInertia`→ `Mat3_ MomentInertia`（3D 转动惯量是张量，各轴不同）
	 *  - `FLOAT_ invMomentInertia` → `Mat3_ invMomentInertia`
	 *  - `FLOAT_ torque`       → `Vec3_ torque`
	 *  - `AddTorque(FLOAT_,FLOAT_)` → `AddTorque(const Vec3_&)`（2D 的"力臂×力"标量
	 *    在 3D 是叉积向量，且力臂本身是向量而非标量）
	 *  - `PFGetAngle/PFSetAngle` → `PFGetRotation/PFSetRotation`
	 *
	 * **保留标量 `angle` 的只读投影 `GetYaw()`**：2D 场景中大量逻辑（如精灵朝向、
	 * 面向判断）只关心绕 Y 轴的偏航角，提供便捷换算而不必到处写四元数解算。
	 */
	class PhysicsAngle3D : public PhysicsParticle3D
	{
	public:
		/**
		 * @brief 转动惯量张量（**物体局部坐标系**）
		 * @details 物体抵抗旋转的能力，与质量分布和形状有关（3×3 对称正定矩阵）
		 * @warning 这是**局部**张量，不随物体旋转改变。世界系的逆张量请用
		 *          `GetWorldInvInertia()` 获取。2D 的标量 MomentInertia 之所以
		 *          能直接当世界系用，是因为 2D 只允许绕 Z 轴旋转、且
		 *          Izz 与朝向无关；3D 绕任意轴旋转时张量随朝向变化，
		 *          必须做 `R·I⁻¹·Rᵀ` 共轭变换，否则旋转中的物体角响应错误。
		 */
		Mat3_ MomentInertia{1.0f};

		/**
		 * @brief 逆转动惯量张量（**物体局部坐标系**）
		 * @details 转动惯量张量的逆，用于优化计算，避免每步求逆
		 * @note 静态/无限惯量物体为零矩阵
		 */
		Mat3_ invMomentInertia{1.0f};

		/**
		 * @brief 旋转朝向
		 * @details 物体在世界坐标系中的朝向四元数（单位四元数）
		 */
		Quat_ rotation{1.0f, 0.0f, 0.0f, 0.0f};

		/**
		 * @brief 角速度
		 * @details 物体的旋转速度，方向为旋转轴，模长为角速率（弧度/秒）
		 */
		Vec3_ angleSpeed{0.0f};

		/**
		 * @brief 扭矩
		 * @details 作用在物体上的旋转力矩向量，导致物体旋转
		 */
		Vec3_ torque{0.0f};

		/**
		 * @brief 碰撞半径
		 * @details 用于碰撞检测的半径，定义了物体的碰撞边界
		 * @note 默认 0：三参构造不传半径的派生类若忘记回填，读到的是确定值而非堆垃圾
		 */
		FLOAT_ radius = 0;

#if PhysicsBlock_Serialization3D
		/**
		 * @brief 从 JSON 数据构造物理角度对象
		 * @param data JSON 数据
		 */
		PhysicsAngle3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data);

		/**
		 * @brief 将物理角度对象序列化为 JSON
		 * @param data 输出的 JSON 数据
		 */
		virtual void JsonSerialization(nlohmann::json_abi_v3_12_0::basic_json<> &data);

		/**
		 * @brief 从 JSON 数据反序列化物理角度对象
		 * @param data JSON 数据
		 */
		virtual void JsonContrarySerialization(const nlohmann::json_abi_v3_12_0::basic_json<> &data);
#endif

		/**
		 * @brief 构造函数
		 * @param Pos 位置
		 * @param Mass 质量
		 * @param Friction 摩擦因数
		 * @param Radius 碰撞半径
		 */
		PhysicsAngle3D(Vec3_ Pos, FLOAT_ Mass, FLOAT_ Friction, FLOAT_ Radius);

		/**
		 * @brief 构造函数（无半径参数）
		 * @param Pos 位置
		 * @param Mass 质量
		 * @param Friction 摩擦因数，默认为0.2
		 */
		PhysicsAngle3D(Vec3_ Pos, FLOAT_ Mass, FLOAT_ Friction = 0.2);

		/**
		 * @brief 构造函数（仅位置参数）
		 * @param Pos 位置
		 */
		PhysicsAngle3D(Vec3_ Pos);

		/**
		 * @brief 析构函数
		 */
		virtual ~PhysicsAngle3D();

		/**
		 * @brief 受力（移动，旋转）
		 * @param Pos 受力位置（世界位置）
		 * @param Force 力向量
		 * @details 同时影响物体的移动和旋转。3D 力矩为 `(Pos - pos) × Force`
		 */
		virtual void AddForce(Vec3_ Pos, Vec3_ Force) override;

		/**
		 * @brief 受力（移动）
		 * @param Force 力向量
		 * @details 仅影响物体的移动，不影响旋转
		 */
		virtual void AddForce(Vec3_ Force) override;

		/**
		 * @brief 施加质心冲量（瞬时改变线速度）
		 * @param impulse 冲量向量（Δv = invMass * impulse）
		 */
		virtual void ApplyImpulse(const Vec3_ &impulse) override;

		/**
		 * @brief 施加带受力点的冲量（同时产生角速度）
		 * @param impulse 冲量向量
		 * @param worldPoint 受力点（世界坐标）
		 * @details 线速度 Δv = invMass * impulse；角速度 Δω = invI * ((worldPoint-pos) × impulse)，
		 *          其中 invI 是 3×3 张量（矩阵乘向量）。用于带自旋的击退效果。 */
		virtual void ApplyImpulse(const Vec3_ &impulse, const Vec3_ &worldPoint) override;

		/**
		 * @brief 施加纯角冲量（瞬时改变角速度）
		 * @param torqueImpulse 角冲量向量（Δω = invI * torqueImpulse） */
		virtual void ApplyTorqueImpulse(const Vec3_ &torqueImpulse) override;

		/**
		 * @brief 受力（旋转）
		 * @param ArmForce 力矩向量（力臂 × 力）
		 * @details 仅影响物体的旋转，不影响移动。
		 * @note 2D 版签名为 `AddTorque(FLOAT_ ArmForce, FLOAT_ Force)`（标量力臂 × 标量力），
		 *       3D 改为直接传力矩向量 `Vec3_`——3D 中"力臂"本身是向量，
		 *       拆成两个标量参数无法表达绕任意轴的力矩。 */
		void AddTorque(const Vec3_ &ArmForce);

		/**
		 * @brief 根据外力和时间更新速度和角速度
		 * @param time 时间差（秒）
		 * @param Ga 重力加速度向量
		 */
		virtual void PhysicsSpeed(FLOAT_ time, Vec3_ Ga);

		/**
		 * @brief 根据速度和时间更新位置和朝向
		 * @param time 时间差（秒）
		 * @param Ga 重力加速度向量
		 */
		virtual void PhysicsPos(FLOAT_ time, Vec3_ Ga);

		/**
		 * @brief 获取逆转动惯量张量（世界坐标系）
		 * @return `R · I_local⁻¹ · Rᵀ`
		 * @details 3D 刚体的惯量张量随朝向变化，角冲量/角速度/力矩都在世界系中
		 *          表达，因此所有"用 invI 乘世界系向量"的地方都必须用这个
		 *          共轭变换后的张量，不能直接用局部的 invMomentInertia。
		 *          （2D 标量版无此问题：Izz 与朝向无关。）
		 */
		Mat3_ WorldInvInertia() const
		{
			const Mat3_ R = glm::mat3_cast(rotation);
			return R * invMomentInertia * glm::transpose(R);
		}

		/*=========PhysicsFormwork3D=========*/
		/**
		 * @brief 获取逆转动惯量张量（世界坐标系）
		 * @return 3×3 世界系逆转动惯量张量
		 */
		virtual Mat3_ PFGetInvI() override { return WorldInvInertia(); }

		/**
		 * @brief 获取旋转朝向
		 * @return 当前朝向四元数
		 */
		virtual Quat_ PFGetRotation() override { return rotation; }

		/**
		 * @brief 设置旋转朝向
		 * @param r 目标朝向四元数（内部归一化）
		 */
		virtual void PFSetRotation(const Quat_ &r) override { rotation = glm::normalize(r); }

		/**
		 * @brief 获取角速度引用
		 * @return 角速度向量的引用
		 */
		virtual Vec3_ &PFAngleSpeed() override { return angleSpeed; };

		/**
		 * @brief 获取碰撞半径
		 * @return 碰撞半径
		 */
		virtual FLOAT_ PFGetCollisionR() override { return radius; }

		/**
		 * @brief 便捷：取绕 Y 轴的偏航角（弧度）
		 * @details 从四元数投影出 yaw，供只需要平面朝向的逻辑使用
		 */
		FLOAT_ GetYaw() const;
	};

}
