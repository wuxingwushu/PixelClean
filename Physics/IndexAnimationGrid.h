#pragma once
#include "ObjectCollision.h"
#include "ObjectCollision.h"
#include "GridDecorator.h"

namespace SquarePhysics {

	class IndexAnimationGrid :public ObjectCollision
	{
	public:
		IndexAnimationGrid(
			unsigned int x,
			unsigned int y,
			unsigned int SideLength,
			unsigned int Frames);
		~IndexAnimationGrid();

		virtual inline PixelAttribute* at(glm::ivec2 pos) {
			if (AnimationIndex[pos.y * mNumberY + pos.x + AnimationDeviation] >= 0) {
				return &mPixelAttributeS[AnimationIndex[pos.y * mNumberY + pos.x + AnimationDeviation]];
			}
			return &mPixelAttributeS[mNumberX * mNumberX - 1];
		}

		inline void SetCurrentFrame(unsigned int frame) {
			CurrentFrame = frame;
			AnimationDeviation = CurrentFrame * mNumberX * mNumberY;
		}

		void SetAnimationIndex(int* Index) {
			AnimationIndex = Index;
		}
		/*++++++++++++++++++++++++++++++++       碰撞有关       ++++++++++++++++++++++++++++++++*/

		//计算当前碰撞体的 外骨架
		virtual void OutlineCalculate();

		//判断点是否符合骨架的条件
		virtual inline void OutlinePointJudge(int x, int y);

		//有多少个外骨架点
		virtual unsigned int GetOutlinePointSize() { return OutlinePointSizeS[CurrentFrame]; }
		//获取第 I 个外骨架点
		virtual glm::vec2 GetOutlinePointSet(unsigned int i) { return { mOutlinePointSet[CurrentFrame][i].x - OriginX, mOutlinePointSet[CurrentFrame][i].y - OriginY }; }

	private:
		unsigned int mFrames = 0;
		int* AnimationIndex{ nullptr };//动画像素索引
		unsigned int AnimationDeviation = 0;//动画索引偏移
		unsigned int CurrentFrame = 0;//当前帧

		unsigned int* OutlinePointSizeS{ nullptr };//点集的数量
		glm::vec2** mOutlinePointSet{ nullptr };//外包裹点集
	};

}