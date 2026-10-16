#pragma once
#include "PhysicsAngle3D.hpp"
#include "BaseOutline3D.hpp"
#include "BaseDefine3D.h"
#include "PhysicsFormwork3D.hpp"

namespace PhysicsBlock3D
{

	/**
	 * @brief 物理形状类（3D）
	 * @details 表示一个具有质量、位置、旋转和碰撞检测能力的物理形状
	 * 继承自 PhysicsAngle3D（处理物理角度和位置）和 BaseOutline3D（处理网格和轮廓）
	 *
	 * @note 由 2D 版 PhysicsShape 改写。维度变化：
	 *  - `glm::ivec2 Size` → `IVec3_ Size`（三维体素块）
	 *  - `CentreMass`/`CentreShape`：Vec2_ → Vec3_
	 *  - 转动惯量：标量 → 3×3 张量（`I = Σ m_i ((r·r)E − r⊗r)`）
	 *  - `RayCollide(Pos, FLOAT_ Angle)` → `RayCollide(Pos, Vec3_ Dir)`（方向向量）
	 *  - `PsBresenhamDetection` → `PsDDADetection`（三维 DDA）
	 */
	class PhysicsShape3D : public PhysicsAngle3D, public BaseOutline3D
	{
#if PhysicsBlock_Serialization3D
	public:
		/**
		 * @brief 序列化虚函数
		 */
		SerializationVirtualFunction3D;

		/**
		 * @brief 从 JSON 数据构造物理形状
		 * @param data JSON 数据
		 */
		PhysicsShape3D(const nlohmann::json_abi_v3_12_0::basic_json<> &data)
		    : PhysicsAngle3D(data), BaseOutline3D(data)
		{
			type = PhysicsObjectEnum::shape;
			UpdateAll();
			ContrarySerializationVec3(data, pos);
			ContrarySerializationVec3(data, OldPos);
		};
#endif
	public:
		/**
		 * @brief 质心（相对网格原点的局部坐标）
		 * @details 形状的质量中心，用于物理计算
		 */
		Vec3_ CentreMass{0};

		/**
		 * @brief 几何中心（相对网格原点的局部坐标）
		 * @details 形状的几何中心，用于碰撞检测和渲染
		 */
		Vec3_ CentreShape{0};

	public:
		/**
		 * @brief 构造函数
		 * @param Pos 形状的初始位置
		 * @param Size 网格大小（三维）
		 */
		PhysicsShape3D(Vec3_ Pos, IVec3_ Size);

		/**
		 * @brief 析构函数
		 */
		virtual ~PhysicsShape3D();

		/**
		 * @brief 获取指定索引的网格块
		 * @param i 网格索引
		 * @return 网格块的引用
		 */
		GridBlock3D &Gridat(unsigned int i) { return at(i); }

		/**
		 * @brief 检测点是否点击到网格
		 * @param pos 点的世界位置
		 * @return 碰撞结果信息，包含是否碰撞和碰撞的网格位置
		 */
		CollisionInfoI3D DropCollision(Vec3_ pos);

		/**
		 * @brief 更新形状的物理信息
		 * @details 更新以下信息：
		 * - 总重量（质量）
		 * - 几何中心
		 * - 质心
		 * - 转动惯量张量
		 * @note 当质量为 FLOAT_MAX 时，形状被视为不可移动
		 */
		void UpdateInfo();

		/**
		 * @brief 计算碰撞半径
		 * @details 根据形状的轮廓计算碰撞半径
		 */
		void UpdateCollisionR();

		/**
		 * @brief 更新全部信息
		 * @details 调用 UpdateInfo()、UpdateOutline() 和 UpdateCollisionR() 更新所有信息
		 */
		void UpdateAll();

		/**
		 * @brief 几何形状靠近指定点
		 * @param drop 目标点的位置
		 */
		void ApproachDrop(Vec3_ drop);

		/*=========BaseGrid3D=========*/

		/**
		 * @brief 射线碰撞检测
		 * @param Pos 射线起点
		 * @param Dir 射线方向向量（内部归一化）
		 * @return 碰撞结果信息，包含是否碰撞和碰撞的精确位置
		 * @note 2D 版签名是 `RayCollide(Vec2_ Pos, FLOAT_ Angle)`；
		 *       3D 用方向向量而非角度——三维方向需要两个角（方位+俯仰），
		 *       直接传向量既无歧义也免去角度换算。
		 */
		CollisionInfoD3D RayCollide(Vec3_ Pos, Vec3_ Dir);

		/**
		 * @brief 线段碰撞检测（整数坐标）
		 * @param start 线段的起始位置（局部网格坐标）
		 * @param end 线段的结束位置（局部网格坐标）
		 * @return 碰撞结果信息
		 */
		CollisionInfoI3D PsDDADetection(IVec3_ start, IVec3_ end);

		/**
		 * @brief 线段碰撞检测（浮点坐标，世界系）
		 * @param start 线段的起始位置（世界坐标）
		 * @param end 线段的结束位置（世界坐标）
		 * @return 碰撞结果信息，包含是否碰撞和碰撞的精确位置（世界坐标）
		 */
		CollisionInfoD3D PsDDADetection(Vec3_ start, Vec3_ end);

		/*=========PhysicsFormwork3D=========*/
	};

}
