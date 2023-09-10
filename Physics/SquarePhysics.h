#pragma once
#include "PhysicsCalculate.h"			//通用的计算
#include "StructuralComponents.h"		//设置的结构体
#include "ObjectDecorator.h"			//Class修饰体
#include "FixedSizeTerrain.h"			//固定大小的网格地图（静态）（物理模块）
#include "MoveTerrain.h"				//固定大小的网格地图群
#include "ObjectCollision.h"			//固定大小的网格（动态）（物理模块）
#include "PixelCollision.h"				//格子物理（动态）（物理模块）
#include "../Tool/ContinuousData.h"		//动态连续储存


namespace SquarePhysics {


	class SquarePhysics
	{
	public:
		//初始化（ObjectNumber 和 PixelNumber 上限）
		SquarePhysics(unsigned int ObjectNumber, unsigned int PixelNumber);
		~SquarePhysics();

		//添加Object
		void AddObjectCollision(ObjectCollision* LObjectCollision) { mObjectCollisionS->add(LObjectCollision); }
		//移除Object
		void RemoveObjectCollision(ObjectCollision* LObjectCollision) {
			ObjectCollision** ObjectCollisionS = mObjectCollisionS->Data();
			for (size_t i = 0; i < mObjectCollisionS->GetNumber(); i++)
			{
				if (ObjectCollisionS[i] == LObjectCollision) {
					mObjectCollisionS->Delete(i);
					return;
				}
			}
		}
		//添加Pixel
		void AddPixelCollision(PixelCollision* LPixelCollision) { mPixelCollisionS->add(LPixelCollision); }
		//移除Object
		void RemovePixelCollision(PixelCollision* LPixelCollision) {
			PixelCollision** PixelCollisionS = mPixelCollisionS->Data();
			for (size_t i = 0; i < mPixelCollisionS->GetNumber(); i++)
			{
				if (PixelCollisionS[i] == LPixelCollision) {
					mPixelCollisionS->Delete(i);
					return;
				}
			}
		}
		//设置地图
		void SetFixedSizeTerrain(GridDecorator* LTerrain) { mTerrain = LTerrain; }
		//物理模拟
		void PhysicsSimulation(float TimeStep);

	private:
		GridDecorator* mTerrain = nullptr;					//地图
		ContinuousData<ObjectCollision*>* mObjectCollisionS = nullptr;	//玩家，物品，碎片
		ContinuousData<PixelCollision*>* mPixelCollisionS = nullptr;	//子弹，碎片
	};

	
}
