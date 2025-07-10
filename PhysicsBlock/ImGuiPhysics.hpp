#pragma once
#include "../ImGui/imgui.h"
#include "PhysicsParticle.hpp"
#include "PhysicsShape.hpp"
#include "PhysicsWorld.hpp"

namespace PhysicsBlock 
{
	/**
	 * @brief 将 PhysicsShape 变量UI显示
	 * @param Object PhysicsShape 指针
	 * @return 是否修改导致需要更新对象信息 */
	bool PhysicsShapeUI(PhysicsShape* Object);

	/**
	 * @brief 将 PhysicsParticle 变量UI显示
	 * @param Object PhysicsParticle 指针
	 * @return 是否修改导致需要更新对象信息 */
	bool PhysicsShapeUI(PhysicsParticle* Object);

	/**
	 * @brief 将 PhysicsWorld 变量UI显示
	 * @param Object PhysicsWorld 指针
	 * @return 是否修改导致需要更新对象信息 */
	bool PhysicsShapeUI(PhysicsWorld* Object);

}