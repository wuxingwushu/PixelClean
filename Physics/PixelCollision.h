#pragma once
#include "StructuralComponents.h"
#include "ObjectDecorator.h"

//PixelCollision
namespace SquarePhysics {

	class PixelCollision :public ObjectDecorator
	{
		typedef void (*_PixelCollisionCallback)(PixelCollision* PPixelCollision, void* mclass);

	public:
		PixelCollision(unsigned int SideLength);
		~PixelCollision();

		void SetCollisionCallback(_PixelCollisionCallback CollisionCallback, void* mclass) {
			mCollisionCallback = CollisionCallback;
			mClass = mclass;
		}

		void CollisionCallback(PixelCollision* PPixelCollision) {
			mCollisionCallback(PPixelCollision, mClass);
		}

		_PixelCollisionCallback mCollisionCallback = nullptr; //碰撞事件的回调函数


	private:
		void* mClass = nullptr;
		unsigned int mSideLength;
		//PixelAttribute mPixelAttribute;
	};

}