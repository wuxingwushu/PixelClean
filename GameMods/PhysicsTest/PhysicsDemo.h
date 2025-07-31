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

	// 杠杆连接
	void PhysicsDemo7(PhysicsWorld** myPhysicsWorld, Camera* mCamera);

	// 橡皮筋受力
	void PhysicsDemo8(PhysicsWorld** myPhysicsWorld, Camera* mCamera);

	// 绳子与地形碰撞
	void PhysicsDemo9(PhysicsWorld **myPhysicsWorld, Camera *mCamera);

	// 圆
	void PhysicsDemo10(PhysicsWorld **myPhysicsWorld, Camera *mCamera);

	// 圆压力测试
	void PhysicsDemo11(PhysicsWorld **myPhysicsWorld, Camera *mCamera);

	// 绳子牛顿摆
	void PhysicsDemo12(PhysicsWorld **myPhysicsWorld, Camera *mCamera);

	// 关节牛顿摆
	void PhysicsDemo13(PhysicsWorld **myPhysicsWorld, Camera *mCamera);

	// 摩擦力
	void PhysicsDemo14(PhysicsWorld **myPhysicsWorld, Camera *mCamera);


	typedef void (*Funtypedef)(PhysicsWorld** myPhysicsWorld, Camera* mCamera);
	static Funtypedef DemoFunS[] = { 
		PhysicsDemo0, 
		PhysicsDemo1, 
		PhysicsDemo2, 
		PhysicsDemo3, 
		PhysicsDemo4, 
		PhysicsDemo5, 
		PhysicsDemo6, 
		PhysicsDemo7, 
		PhysicsDemo8, 
		PhysicsDemo9, 
		PhysicsDemo10, 
		PhysicsDemo11, 
		PhysicsDemo12, 
		PhysicsDemo13,
		PhysicsDemo14
	 };
	static const char* DemoNameS[] = { 
		"外骨骼计算", 
		"几何重心", 
		"正方形堆叠", 
		"正方形堆叠金字塔", 
		"点和正方形", 
		"关节", 
		"铁链", 
		"杠杆连接", 
		"橡皮筋受力", 
		"绳子与地形碰撞", 
		"圆", 
		"圆压力测试", 
		"绳子牛顿摆", 
		"关节牛顿摆",
		"摩擦力"
	 };

}
