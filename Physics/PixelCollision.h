#pragma once
#include "StructuralComponents.h"
#include "ObjectDecorator.h"
#include "DestroyMode.h"

//PixelCollision
namespace SquarePhysics {

	class PixelCollision :public ObjectDecorator
	{
		typedef void (*_PixelCollisionCallback)(PixelCollision* PPixelCollision, void* mclass);

	public:
		PixelCollision(unsigned int SideLength);
		~PixelCollision();

		//设置回调
		void SetCollisionCallback(_PixelCollisionCallback CollisionCallback, void* Data) {
			mCollisionCallback = CollisionCallback;
			mData = Data;
		}

		//调用回调
		void CollisionCallback(PixelCollision* PPixelCollision) {
			mCollisionCallback(PPixelCollision, mData);
		}

		_PixelCollisionCallback mCollisionCallback = nullptr; //碰撞事件的回调函数

		//获取边长
		unsigned int GetSideLength() {
			return mSideLength;
		}

	private:
		void* mData = nullptr;//回调数据
		unsigned int mSideLength;//边长
	};

}