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

		virtual inline PixelAttribute* at(glm::uvec2 pos) noexcept {
			int Index = AnimationIndex[(pos.y * mNumber.x) + pos.x + AnimationDeviation];
			if (Index >= 0) {
				return &mPixelAttributeS[Index];
			}
			return &LS_PixelAttribute;
		}

		inline void SetCurrentFrame(unsigned int frame) noexcept {
			CurrentFrame = frame;
			AnimationDeviation = CurrentFrame * (mNumber.x * mNumber.y);
		}

		inline void SetAnimationIndex(int* Index) noexcept {
			AnimationIndex = Index;
		}
		/*++++++++++++++++++++++++++++++++       碰撞有关       ++++++++++++++++++++++++++++++++*/

		//计算当前碰撞体的 外骨架
		virtual void OutlineCalculate();

		//判断点是否符合骨架的条件
		virtual inline void OutlinePointJudge(int x, int y);

		//有多少个外骨架点
		virtual inline unsigned int GetOutlinePointSize() noexcept { return OutlinePointSizeS[CurrentFrame]; }
		//获取第 I 个外骨架点
		virtual inline glm::dvec2 GetOutlinePointSet(unsigned int i) noexcept { return { mOutlinePointSet[CurrentFrame][i].x - Origin.x, mOutlinePointSet[CurrentFrame][i].y - Origin.y }; }

	private:
		unsigned int mFrames = 0;
		int* AnimationIndex{ nullptr };//动画像素索引
		unsigned int AnimationDeviation = 0;//动画索引偏移
		unsigned int CurrentFrame = 0;//当前帧

		unsigned int* OutlinePointSizeS{ nullptr };//点集的数量
		glm::dvec2** mOutlinePointSet{ nullptr };//外包裹点集


	//空
		PixelAttribute LS_PixelAttribute;
	};

}