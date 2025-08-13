#pragma once
#include "PhysicsParticle.hpp"
#include "PhysicsShape.hpp"
#include "PhysicsCircle.hpp"
#include "PhysicsLine.hpp"
#include "PhysicsWorld.hpp"

// 辅助声明变量
#define AuxiliaryExternBoolColor(Name)  \
	extern bool Auxiliary_##Name##Bool; \
	extern float Auxiliary_##Name##Color[4];

// 名字字符串化
#define TOSIRING(str) #str

// 辅助实现变量
#define AuxiliaryBoolColor(Name)         \
	bool Auxiliary_##Name##Bool = false; \
	float Auxiliary_##Name##Color[4] = {0, 0, 0, 0};

// 辅助读取信息
#define AuxiliaryReadBoolColor(Name)                                                           \
	Auxiliary_##Name##Bool = Ini.Get<bool>("Auxiliary", TOSIRING(Name##Bool));                 \
	std::vector<float> Name##Color = Ini.GetVector<float>("Auxiliary", TOSIRING(Name##Color)); \
	for (size_t i = 0; i < Name##Color.size(); ++i)                                            \
	{                                                                                          \
		Auxiliary_##Name##Color[i] = Name##Color[i];                                           \
	}

// 辅助保存信息
#define AuxiliaryStorageBoolColor(Name)                                         \
	Ini.UpdateEntry("Auxiliary", TOSIRING(Name##Bool), Auxiliary_##Name##Bool); \
	Ini.UpdateEntry("Auxiliary", TOSIRING(Name##Color), std::vector<float>(Auxiliary_##Name##Color, Auxiliary_##Name##Color + 4));

#define ColorToVec4(color) \
	glm::vec4 { color[0], color[1], color[2], color[3] }

namespace PhysicsBlock
{
	// 声明变量
	// 物理对象基础信息
	AuxiliaryExternBoolColor(Pos);		   // 位置
	AuxiliaryExternBoolColor(OldPos);	   // 旧位置
	AuxiliaryExternBoolColor(Angle);	   // 角度
	AuxiliaryExternBoolColor(Speed);	   // 速度
	AuxiliaryExternBoolColor(Force);	   // 受力
	AuxiliaryExternBoolColor(CentreMass);  // 质心
	AuxiliaryExternBoolColor(CentreShape); // 几何中心
	AuxiliaryExternBoolColor(Outline);	   // 外骨骼点
	// 碰撞信息
	AuxiliaryExternBoolColor(CollisionDrop);				  // 碰撞点
	AuxiliaryExternBoolColor(SeparateNormalVector);			  // 分离法向量
	AuxiliaryExternBoolColor(CollisionDropToCenterOfGravity); // 碰撞点到两个物体重心的连线
	AuxiliaryExternBoolColor(GridDivided);					  // 被选中的物体被划分网格到的网格位置

	// 读取物理辅助显示信息
	void AuxiliaryInfoRead();

	// 保存物理辅助显示信息
	void AuxiliaryInfoStorage();

	// 物理辅助显示参数的控制UI
	void PhysicsAuxiliaryColorUI();

	/**
	 * @brief 将 PhysicsLine 变量UI显示
	 * @param Object PhysicsLine 指针
	 * @return 是否修改导致需要更新对象信息 */
	bool PhysicsUI(PhysicsLine *Object);

	/**
	 * @brief 将 PhysicsCircle 变量UI显示
	 * @param Object PhysicsCircle 指针
	 * @return 是否修改导致需要更新对象信息 */
	bool PhysicsUI(PhysicsCircle *Object);

	/**
	 * @brief 将 PhysicsShape 变量UI显示
	 * @param Object PhysicsShape 指针
	 * @return 是否修改导致需要更新对象信息 */
	bool PhysicsUI(PhysicsShape *Object);

	/**
	 * @brief 将 PhysicsAngle 变量UI显示
	 * @param Object PhysicsAngle 指针
	 * @return 是否修改导致需要更新对象信息 */
	bool PhysicsUI(PhysicsAngle *Object);

	/**
	 * @brief 将 PhysicsParticle 变量UI显示
	 * @param Object PhysicsParticle 指针
	 * @return 是否修改导致需要更新对象信息 */
	bool PhysicsUI(PhysicsParticle *Object);

	/**
	 * @brief 将 PhysicsWorld 变量UI显示
	 * @param Object PhysicsWorld 指针
	 * @return 是否修改导致需要更新对象信息 */
	bool PhysicsUI(PhysicsWorld *Object);

}