#pragma once
#include "../../PhysicsBlock/PhysicsWorld.hpp"
#include "../../PhysicsBlock/MapStatic.hpp"
#include "../../Camera.h"

namespace PhysicsBlock
{
	// 外骨骼计算
	void PhysicsDemo0(PhysicsWorld **myPhysicsWorld, Camera *mCamera);

	// 几何重心
	void PhysicsDemo1(PhysicsWorld** myPhysicsWorld, Camera* mCamera);

	// 正方形堆叠
	void PhysicsDemo2(PhysicsWorld** myPhysicsWorld, Camera* mCamera);

	// 正方形堆叠金字塔
	void PhysicsDemo3(PhysicsWorld** myPhysicsWorld, Camera* mCamera);

	// 点 和 正方形
	void PhysicsDemo4(PhysicsWorld** myPhysicsWorld, Camera* mCamera);

	// 关节
	void PhysicsDemo5(PhysicsWorld** myPhysicsWorld, Camera* mCamera);

	// 铁链
	void PhysicsDemo6(PhysicsWorld** myPhysicsWorld, Camera* mCamera);

	// 绳子
	void PhysicsDemo7(PhysicsWorld** myPhysicsWorld, Camera* mCamera);

	// 橡皮筋受力
	void PhysicsDemo8(PhysicsWorld** myPhysicsWorld, Camera* mCamera);


	typedef void (*Funtypedef)(PhysicsWorld** myPhysicsWorld, Camera* mCamera);
	static Funtypedef DemoFunS[] = { PhysicsDemo0, PhysicsDemo1, PhysicsDemo2, PhysicsDemo3, PhysicsDemo4, PhysicsDemo5, PhysicsDemo6, PhysicsDemo7, PhysicsDemo8 };
	static const char* DemoNameS[] = { "外骨骼计算", "几何重心", "正方形堆叠", "正方形堆叠金字塔", "点和正方形", "关节", "铁链", "绳子", "橡皮筋受力" };

}
