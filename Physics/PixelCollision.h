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
		virtual ~PixelCollision();

		//设置回调
		inline void SetCollisionCallback(_PixelCollisionCallback CollisionCallback, void* Data) noexcept {
			mCollisionCallback = CollisionCallback;
			mData = Data;
		}

		//调用回调
		inline void CollisionCallback(PixelCollision* PPixelCollision) noexcept {
			mCollisionCallback(PPixelCollision, mData);
		}

		_PixelCollisionCallback mCollisionCallback = nullptr; //碰撞事件的回调函数

		//获取边长
		inline unsigned int GetSideLength() noexcept {
			return mSideLength;
		}

	private:
		void* mData = nullptr;//回调数据
		unsigned int mSideLength;//边长
	};

}