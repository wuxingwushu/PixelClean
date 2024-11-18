#pragma once
#include <glm/glm.hpp>

//一个几何图形类型说明
//typedef int Typomorphic;
//#define	Circular			0	//圆形
//#define	Triangle			3	//三角形
//#define	Square				4	//四边形

namespace SquarePhysics {

	class ObjectDecorator;

	//说明整体的碰撞结构
	//struct Entity {
	//	Typomorphic mTypomorphic;
	//	glm::vec2 Entitydata[4];//有什么几何图像构成
	//	glm::vec2 pos;//位置
	//	float angle;//当前角度
	//	glm::vec2 power;//受力
	//	glm::vec2 focus;//重心
	//	float quality = 1.0f;//质量		***质量为零时，定义为静态体
	//	float friction = 0.3f;//摩擦力

	//	//粗糙碰撞体
	//	glm::vec2 CircumscribedCircle_pos;//外接圆位置
	//	float CircumscribedCircle_radius;//外接圆半径
	//};

	struct PixelAttribute {
		bool Collision = false;				//是否可穿透
		float FrictionCoefficient = 0.6f;	//摩擦系数
		unsigned int Type = 0;				//这是什么类型对象
	};

	//碰撞返回信息
	struct CollisionInfo {
		bool Collision = false;	//是否碰撞
		glm::dvec2 Pos{ 0 };		//碰撞位置
		double NormalAngle = 0;	//法线角度
	};

	//分解力
	struct DecompositionForce {
		glm::dvec2 Parallel{ 0 };//平行
		glm::dvec2 Vertical{ 0 };//垂直
	};

	//受力
	struct ObjectSufferForce {
		ObjectDecorator* Object = nullptr;	//受力对象
		glm::dvec2 ArmOfForce{ 0 };			//力臂
	};

	//破坏模式回调函数
	//typedef void (*_DestroyModeCallback)(int x, int y, void* mclass, bool Bool);

	//破坏点回调函数的类型
	//typedef void (*_TerrainCollisionCallback)(int x, int y, bool Bool, void* mclass);
}