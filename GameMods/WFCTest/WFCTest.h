#pragma once

/*
 * WFCTest — 基于 WaveFunctionCollapse 库的 2D 城市地图生成测试 Demo
 *
 * 核心演示内容：
 *   1. OverlappingWFC — 使用重叠模式从输入图像生成更大的输出图像
 *   2. 输入图案：10x10 的城镇地图，包含 7 种地形：
 *      0=草地  1=楼房  2=水域  3=道路(横)  4=道路(竖)  5=桥梁  6=树林
 *   3. 输出：60x40 的自动生成城市地图，渲染为彩色方块网格
 *   4. 按 1 切换周期性输出，按 2 重新生成
 *
 * 操作方式：
 *   W/A/S/D — 平移相机
 *   滚轮 — 缩放相机
 *   1 — 切换 periodic_output 选项
 *   2 — 重新生成 WFC
 */

#include "../Configuration.h"
#include "../GameMods.h"
#include "../../Environment/WaveFunctionCollapse/include/overlapping_wfc.hpp"
#include <random>

namespace GAME
{

	class WFCTest : public GameMods, Configuration
	{
	public:
		WFCTest(Configuration wConfiguration);
		~WFCTest();

		virtual void MouseMove(double xpos, double ypos);
		virtual void MouseRoller(int z);
		virtual void KeyDown(GameKeyEnum moveDirection);
		virtual void GameCommandBuffers(unsigned int Format_i);
		virtual void GameLoop(unsigned int mCurrentFrame);
		virtual void GameRecordCommandBuffers();
		virtual void GameStopInterfaceLoop(unsigned int mCurrentFrame);
		virtual void GameTCPLoop();
		virtual void GameUI();

	private:
		// 生成 WFC 输出
		void GenerateWFC();

		// 渲染 WFC 生成的网格地图
		void RenderWFCMap();

		// 根据像素值获取颜色
		glm::vec4 GetColorForPixel(unsigned pixel) const;

		// 获取地形名称
		const char* GetTerrainName(unsigned pixel) const;

	private:
		VulKan::AuxiliaryVision* mAuxiliaryVision = nullptr;

		// WFC 输入图像
		Array2D<unsigned> mInputPattern;

		// WFC 生成的输出图像
		std::optional<Array2D<unsigned>> mOutput;

		// WFC 选项
		bool mPeriodicOutput = false;
		unsigned mPatternSize = 3;
		unsigned mSymmetry = 8;
		unsigned mOutWidth = 60;
		unsigned mOutHeight = 40;

		// 随机种子
		int mSeed = 42;

		// 相机
		glm::vec3 mCameraTarget = {0, 0, 70};

		// 移动方向
		bool mMoveUp = false;
		bool mMoveDown = false;
		bool mMoveLeft = false;
		bool mMoveRight = false;

		// 是否需要重新生成
		bool mNeedRegenerate = true;
	};

}