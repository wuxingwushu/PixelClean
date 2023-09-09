#pragma once
#include "GridDecorator.h"
#include "../Tool/MovePlate.h"


namespace SquarePhysics {

	template<typename ClassT>
	class MoveTerrain
	{
	public:
		struct RigidBodyAndModel {
			GridDecorator* mGridDecorator = nullptr;
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
		~MoveTerrain();

		void SetCallback(_MoveTerrainGenerateCallback G, void* GData, _MoveTerrainDeleteCallback D, void* DData) {
			mGridDecoratorS->SetCallback(G, GData, D, DData);
		}

		RigidBodyAndModel* GetRigidBodyAndModel(unsigned int x, unsigned int y) {
			return mGridDecoratorS->GetPlate(x,y);
		}

		bool GetFixedCollisionBool(glm::ivec2 dian) {
			glm::ivec2 DA = dian / mSquareSideLength;
			glm::ivec2 XI = dian % mSquareSideLength;
			return mGridDecoratorS->GetPlate(DA.x, DA.y)->mGridDecorator->GetFixedCollisionBool(XI);
		}

		//路径碰撞判断
		[[nodiscard]] CollisionInfo RadialCollisionDetection(glm::ivec2 Start, glm::ivec2 End);

		//更新位置
		void UpDataPos(float x, float y) {
			mGridDecoratorS->UpData(x, y);
		}

	private:
		unsigned int mNumberX;
		unsigned int mNumberY;
		unsigned int mSquareSideLength;
		unsigned int mSideLength;
		MovePlate<RigidBodyAndModel>* mGridDecoratorS = nullptr;
	};


	template<typename ClassT>
	MoveTerrain<ClassT>::MoveTerrain(
		unsigned int NumberX,
		unsigned int NumberY,
		unsigned int SquareSideLength,
		unsigned int SideLength
	):
		mNumberX(NumberX),
		mNumberY(NumberY),
		mSquareSideLength(SquareSideLength),
		mSideLength(SideLength)
	{
		mGridDecoratorS = new MovePlate<RigidBodyAndModel>(mNumberX, mNumberY, mSquareSideLength, mNumberX / 2, mNumberY / 2);
		for (size_t ix = 0; ix < NumberX; ix++)
		{
			for (size_t iy = 0; iy < NumberY; iy++)
			{
				mGridDecoratorS->GetPlate(ix, iy)->mGridDecorator = new GridDecorator(mSquareSideLength, mSquareSideLength, mSideLength);
			}
		}
	}

	template<typename ClassT>
	MoveTerrain<ClassT>::~MoveTerrain()
	{

	}

	template<typename ClassT>
	[[nodiscard]] CollisionInfo MoveTerrain<ClassT>::RadialCollisionDetection(glm::ivec2 Start, glm::ivec2 End)
	{
		int dx = abs(End.x - Start.x);
		int dy = abs(End.y - Start.y);
		int sx = (Start.x < End.x) ? 1 : -1;
		int sy = (Start.y < End.y) ? 1 : -1;
		int err = dx - dy;
		int e2;
		while (true) {
			if (GetFixedCollisionBool(Start)) {
				return { true, Start };
			}
			if (Start.x == End.x && Start.y == End.y) {
				return { false, Start };
			}
			e2 = 2 * err;
			if (e2 > -dy) {
				err -= dy;
				Start.x += sx;
			}
			if (GetFixedCollisionBool(Start)) {
				return { true, Start };
			}
			if (Start.x == End.x && Start.y == End.y) {
				return { false, Start };
			}
			if (e2 < dx) {
				err += dx;
				Start.y += sy;
			}
		}
	}
}