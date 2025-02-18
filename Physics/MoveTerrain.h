#pragma once
#include "GridDecorator.h"
#include "../Tool/MovePlate.h"


namespace SquarePhysics {

	template<typename ClassT>
	class MoveTerrain :public GridDecorator
	{
	public:
		struct RigidBodyAndModel {
			GridDecorator mGridDecorator;
			ClassT* mModel = nullptr;
		};

		typedef void (*_MoveTerrainGenerateCallback)(RigidBodyAndModel* mT, int x, int y, void* Data);//生成回调函数
		typedef void (*_MoveTerrainDeleteCallback)(RigidBodyAndModel* mT, void* Data);//销毁回调函数
	
		MoveTerrain(
			unsigned int NumberX,
			unsigned int NumberY,
			unsigned int SquareSideLength,
			unsigned int SideLength
		);
		virtual ~MoveTerrain();

		inline void SetCallback(_MoveTerrainGenerateCallback G, void* GData, _MoveTerrainDeleteCallback D, void* DData) noexcept {
			mGridDecoratorS->SetCallback(G, GData, D, DData);
		}

		inline RigidBodyAndModel* GetRigidBodyAndModel(unsigned int x, unsigned int y) noexcept {
			return mGridDecoratorS->GetPlate(x,y);
		}

		/************************  重写  ************************/
		virtual bool GetFixedCollisionBool(glm::ivec2 pos) {
			RigidBodyAndModel* LRigidBodyAndModel = mGridDecoratorS->CalculateGetPlate(pos.x, pos.y);
			if (LRigidBodyAndModel != nullptr) {
				return LRigidBodyAndModel->mGridDecorator.GetFixedCollisionBool({ pos.x % mSquareSideLength, pos.y % mSquareSideLength });
			}
			return true;
		}

		virtual float GetFrictionCoefficient(glm::ivec2 pos) {
			RigidBodyAndModel* LRigidBodyAndModel = mGridDecoratorS->CalculateGetPlate(pos.x, pos.y);
			if (LRigidBodyAndModel != nullptr) {
				return LRigidBodyAndModel->mGridDecorator.GetFrictionCoefficient({ pos.x % mSquareSideLength, pos.y % mSquareSideLength });
			}
			return 1.0f;
		}

		//调用回调函数
		virtual void CollisionCallback(int x, int y, bool Bool, ObjectDecorator* object) {
			RigidBodyAndModel* LRigidBodyAndModel = mGridDecoratorS->CalculateGetPlate(x, y);
			if (LRigidBodyAndModel != nullptr) {
				LRigidBodyAndModel->mGridDecorator.CollisionCallback(x % mSquareSideLength, y % mSquareSideLength, Bool, object);
			}
		}

		//路径碰撞判断
		virtual CollisionInfo RadialCollisionDetection(glm::ivec2 Start, glm::ivec2 End);

		bool GetFixedCollisionBoolRadial(glm::ivec2 pos) {
			RigidBodyAndModel* LRigidBodyAndModel = mGridDecoratorS->GetPlate(pos.x / mSquareSideLength, pos.y / mSquareSideLength);
			if (LRigidBodyAndModel != nullptr) {
				return LRigidBodyAndModel->mGridDecorator.GetFixedCollisionBool({ pos.x % mSquareSideLength, pos.y % mSquareSideLength });
			}
			return true;
		}
		/************************  写完  ************************/

		

		//更新位置
		inline MovePlate2DInfo UpDataPos(float x, float y) noexcept {
			return mGridDecoratorS->UpData(x, y);
		}

		inline RigidBodyAndModel* CalculateGetRigidBodyAndModel(float x, float y) noexcept {
			return mGridDecoratorS->CalculateGetPlate(x, y);
		}

		inline void SetPos(float x, float y) noexcept {
			mGridDecoratorS->SetPos(x, y);
		}

		inline int GetGridSPosX() noexcept { return mGridDecoratorS->GetPosX(); }
		inline int GetGridSPosY() noexcept { return mGridDecoratorS->GetPosY(); }

	private:
		unsigned int mSquareSideLength;
		MovePlate2D<RigidBodyAndModel>* mGridDecoratorS = nullptr;
	};


	template<typename ClassT>
	MoveTerrain<ClassT>::MoveTerrain(
		unsigned int NumberX,
		unsigned int NumberY,
		unsigned int SquareSideLength,
		unsigned int SideLength
	):
		mSquareSideLength(SquareSideLength),
		GridDecorator(NumberX, NumberY, SideLength, false)
	{
		Origin.x = mNumber.x / 2;
		Origin.y = mNumber.y / 2;
		mGridDecoratorS = new MovePlate2D<RigidBodyAndModel>(mNumber.x, mNumber.y, mSquareSideLength, Origin.x, Origin.y);
		for (size_t ix = 0; ix < mNumber.x; ++ix)
		{
			for (size_t iy = 0; iy < mNumber.y; ++iy)
			{
				new (&mGridDecoratorS->GetPlate(ix, iy)->mGridDecorator) GridDecorator(mSquareSideLength, mSquareSideLength, mSideLength);
			}
		}
	}

	template<typename ClassT>
	MoveTerrain<ClassT>::~MoveTerrain()
	{
		for (size_t ix = 0; ix < mNumber.x; ++ix)
		{
			for (size_t iy = 0; iy < mNumber.y; ++iy)
			{
				mGridDecoratorS->GetPlate(ix, iy)->mGridDecorator.~GridDecorator();
			}
		}
		delete mGridDecoratorS;
	}

	template<typename ClassT>
	[[nodiscard]] CollisionInfo MoveTerrain<ClassT>::RadialCollisionDetection(glm::ivec2 Start, glm::ivec2 End)
	{
		glm::ivec2 Deviation = { mGridDecoratorS->GetPlateX(), mGridDecoratorS->GetPlateY() };
		Start += Deviation;
		End += Deviation;
		int dx = abs(End.x - Start.x);
		int dy = abs(End.y - Start.y);
		int sx = (Start.x < End.x) ? 1 : -1;
		int sy = (Start.y < End.y) ? 1 : -1;
		int err = dx - dy;
		int e2;
		while (true) {
			if (Start.x == End.x && Start.y == End.y) {
				return { false, Start - Deviation, 0 };
			}
			e2 = 2 * err;
			if (e2 > -dy) {
				err -= dy;
				Start.x += sx;
				if (GetFixedCollisionBoolRadial(Start)) {
					return { true, Start - Deviation, 1.57f - (sx == 1 ? 0 : 3.14f) };
				}
			}
			if (e2 < dx) {
				err += dx;
				Start.y += sy;
				if (GetFixedCollisionBoolRadial(Start)) {
					return { true, Start - Deviation, 0 + (sy == 1 ? 0 : 3.14f) };
				}
			}
		}
	}
}