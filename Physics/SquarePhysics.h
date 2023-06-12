#pragma once
#include "PhysicsCalculate.h"			//通用的计算
#include "StructuralComponents.h"		//设置的结构体
#include "ObjectDecorator.h"			//Class修饰体
#include "FixedSizeTerrain.h"			//固定大小的网格地图（静态）（物理模块）
#include "ObjectCollision.h"			//固定大小的网格（动态）（物理模块）
#include "PixelCollision.h"				//格子物理（动态）（物理模块）
#include "../Tool/ContinuousData.h"		//动态连续储存
#include <iostream>


namespace SquarePhysics {


	class SquarePhysics
	{
	public:
		//初始化（ObjectNumber 和 PixelNumber 上限）
		SquarePhysics(unsigned int ObjectNumber, unsigned int PixelNumber);
		~SquarePhysics();

		/*void SetPhysicsSimulationStep(float step) { 
			if (step <= 0)
			{
				std::cerr << "SetPhysicsSimulationStep[Error]: 不可以设置小于等于 0 的值！" << std::endl;
				return;
			}
			mStep = step; 
		}*/

		//添加Object
		void AddObjectCollision(ObjectCollision* LObjectCollision) { mObjectCollisionS->add(LObjectCollision); }
		//添加Pixel
		void AddPixelCollision(PixelCollision* LPixelCollision) { mPixelCollisionS->add(LPixelCollision); }
		//设置地图
		void SetFixedSizeTerrain(FixedSizeTerrain* LFixedSizeTerrain) { mFixedSizeTerrain = LFixedSizeTerrain; }
		//物理模拟
		void PhysicsSimulation(float TimeStep);

		//物理模拟,单单对某个 Pixel 在 FixedSizeTerrain 上模拟
		bool PixelCollisionPhysicsSimulation(float TimeStep, PixelCollision* LPixelCollision);


		ObjectCollision* ObjectS_PixelCollision(glm::ivec2 pos);

	private:
		//float mStep = 1.0f;
		FixedSizeTerrain* mFixedSizeTerrain = nullptr;					//地图
		ContinuousData<ObjectCollision*>* mObjectCollisionS = nullptr;	//玩家，物品，碎片
		ContinuousData<PixelCollision*>* mPixelCollisionS = nullptr;	//子弹，碎片
	};

	
}
