#pragma once
#include "BaseDefine3D.h"
#include "PhysicsFormwork3D.hpp"
#include "BaseSerialization3D.hpp"
#include "PhysicsBlockTypes3D.hpp"

namespace PhysicsBlock3D
{

	/**
	 * @brief 物理粒子类（3D）
	 * @details 表示一个具有质量、位置、速度和受力的物理粒子
	 * 继承自 PhysicsFormwork3D 并实现了序列化接口
	 *
	 * @note 由 2D 版 PhysicsParticle 改写。维度变化：
	 *  - pos/speed/force/OldPos：Vec2_ → Vec3_
	 *  - 无旋转（粒子是点质量），故 PFGetRotation 返回单位四元数、
	 *    PFGetInvI 返回零矩阵、PFAngleSpeed 触发断言。
	 *    （2D 的 PFAngleSpeed 断言后 `return invMass` 属于复用其他成员，
	 *     3D 改为返回一个静态零向量，避免把质量倒数当角速度返回这种
	 *     一旦断言被 NDEBUG 关掉就会静默写坏质量倒数的隐患。）
	 */
	class PhysicsParticle3D : public PhysicsFormwork3D SerializationInherit3D
	{
#if PhysicsBlock_Serialization3D
	public:
		/**
		 * @brief 序列化虚函数
		 */
		SerializationVirtualFunction3D;

		/**
		 * @brief 从 JSON 数据构造粒子
		 * @param data JSON 数据
		 */
		PhysicsParticle3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data);
#endif
	public:
		/**
		 * @brief 静止次数
		 * @details 用于判断粒子是否处于静止状态的计数器
		 */
		unsigned char StaticNum = 0;

		/**
		 * @brief 上一时刻的位置
		 * @note 旧位置不可以在碰撞体内，用于碰撞检测和响应
		 */
		Vec3_ OldPos{0};

		/**
		 * @brief 是否可以更新位置
		 * @details 控制是否允许更新粒子的位置
		 */
		bool OldPosUpDataBool = true;

		/**
		 * @brief 位置
		 * @warning 在形状当中是代表质心在世界坐标的位置
		 */
		Vec3_ pos{0};

		/**
		 * @brief 速度
		 * @details 粒子的线性速度向量
		 */
		Vec3_ speed{0};

		/**
		 * @brief 受力
		 * @details 作用在粒子上的合外力向量
		 */
		Vec3_ force{0};

		/**
		 * @brief 质量
		 * @details 粒子的质量，必须大于0
		 */
		FLOAT_ mass = 0;

		/**
		 * @brief 质量倒数
		 * @details 质量的倒数，用于优化计算，避免除法操作
		 */
		FLOAT_ invMass = 0;

		/**
		 * @brief 摩擦因数
		 * @details 粒子与其他物体接触时的摩擦系数，默认为0.2
		 */
		FLOAT_ friction = 0.2;

		/**
		 * @brief 液体粒子标记
		 * @details 由 PhysicsLiquid3D 设置。液体粒子不参与引擎的碰撞仲裁器
		 *          （粒子↔地图/形状/圆/线），固液交互全部由液体系统自行处理
		 *          （投影 + 反作用 + 浮力 + 阻力），避免深穿透 bias 导致刚体被猛踢/旋转。 */
		bool IsLiquidParticle = false;

	public:
		/**
		 * @brief 构造函数
		 * @param Pos 粒子的初始位置
		 * @param Mass 粒子的质量
		 * @param Friction 摩擦因数，默认为0.2
		 */
		PhysicsParticle3D(Vec3_ Pos, FLOAT_ Mass, FLOAT_ Friction = 0.2);

		/**
		 * @brief 构造函数（无质量参数）
		 * @param Pos 粒子的初始位置
		 * @note 质量默认为0，此时粒子将不受力的影响
		 */
		PhysicsParticle3D(Vec3_ Pos);

		/**
		 * @brief 析构函数
		 */
		virtual ~PhysicsParticle3D();

		/**
		 * @brief 添加一个受力
		 * @param Force 要添加的力向量
		 * @note 力会累加到当前的受力上
		 */
		virtual void AddForce(Vec3_ Force) override;

		/**
		 * @brief 施加质心冲量（瞬时改变速度）
		 * @param impulse 冲量向量（Δv = invMass * impulse）
		 * @note 不可移动粒子（invMass==0）静默忽略；会唤醒静止计数（StaticNum=0） */
		virtual void ApplyImpulse(const Vec3_ &impulse) override;

		/**
		 * @brief 施加纯角冲量
		 * @note 粒子不存在旋转，调用会触发断言 */
		virtual void ApplyTorqueImpulse(const Vec3_ &torqueImpulse) override;

		/*=========PhysicsFormwork3D=========*/

		/**
		 * @brief 根据外力和时间更新速度
		 * @param time 时间差（秒）
		 * @param Ga 重力加速度向量
		 */
		virtual void PhysicsSpeed(FLOAT_ time, Vec3_ Ga);

		/**
		 * @brief 根据速度和时间更新位置
		 * @param time 时间差（秒）
		 * @param Ga 重力加速度向量
		 */
		virtual void PhysicsPos(FLOAT_ time, Vec3_ Ga);

		virtual Vec3_ PFGetPos() { return pos; }

		/**
		 * @brief 获取质量倒数
		 * @return 粒子质量的倒数
		 */
		virtual FLOAT_ PFGetInvMass() { return invMass; }

		/**
		 * @brief 获取质量
		 * @return 粒子的质量
		 */
		virtual FLOAT_ PFGetMass() { return mass; }

		/**
		 * @brief 获取逆转动惯量张量
		 * @return 零矩阵，因为粒子不存在转动惯量
		 * @warning 粒子不存在转动惯量，调用此函数会触发断言
		 */
		virtual Mat3_ PFGetInvI()
		{
			assert(0 && "[Error]: 粒子不存在转动惯量!");
			return Mat3_(0.0f);
		}

		/**
		 * @brief 获取速度引用
		 * @return 粒子的速度向量引用
		 */
		virtual Vec3_ &PFSpeed() { return speed; };

		/**
		 * @brief 获取角速度引用
		 * @return 静态零向量的引用，因为粒子不存在角速度
		 * @warning 粒子不存在角速度，调用此函数会触发断言
		 * @note 3D 版返回独立的静态零向量而非复用 invMass，
		 *       避免断言被关闭时静默破坏质量倒数 */
		virtual Vec3_ &PFAngleSpeed()
		{
			assert(0 && "[Error]: 粒子不存在角速度!");
			static Vec3_ zero(0.0f);
			return zero;
		};
	};

}
