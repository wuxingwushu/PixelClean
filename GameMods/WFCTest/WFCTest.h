#pragma once

/*
 * WFCTest — 基于 WaveFunctionCollapse 库的 2D 城市地图生成测试 Demo
 *
 * 核心演示内容：
 *   1. TilingWFC — 使用显式邻接规则（Tile-based）生成地图
 *   2. 6 种地形块：草地、楼房、水域、道路、桥梁、树林
 *   3. 通过逐条规则定义每种地形四周可以有什么
 *   4. 输出：60x40 的自动生成城市地图，渲染为彩色方块网格
 *   5. 按 1 切换周期性输出，按 2 重新生成
 *
 * 操作方式：
 *   W/A/S/D — 平移相机
 *   滚轮 — 缩放相机
 *   1 — 切换 periodic_output 选项
 *   2 — 重新生成 WFC
 */

#include "../Configuration.h"
#include "../GameMods.h"
#include "../../Environment/WaveFunctionCollapse/include/tiling_wfc.hpp"
#include <random>
#include <vector>
#include <string>

namespace GAME
{

	// 地形类型枚举
	enum TerrainType : unsigned
	{
		Grass = 0,    // 草地
		Building = 1, // 楼房
		Water = 2,    // 水域
		Road = 3,     // 道路
		Bridge = 4,   // 桥梁
		Forest = 5,   // 树林
		TerrainCount = 6
	};

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
		// 初始化所有地形块和图块
		void InitTiles();

		// 定义邻接规则（逐条定义谁可以和谁相邻）
		void InitNeighborRules();

		// 生成 WFC 输出
		void GenerateWFC();

		// 渲染 WFC 生成的网格地图
		void RenderWFCMap();

		// 根据像素值获取颜色
		glm::vec4 GetColorForPixel(unsigned pixel) const;

		// 获取地形名称
		const char* GetTerrainName(unsigned pixel) const;

		// 获取某个地形可以相邻的地形列表（用于 UI 显示）
		std::vector<unsigned> GetAllowedNeighbors(unsigned terrain) const;

	private:
		VulKan::AuxiliaryVision* mAuxiliaryVision = nullptr;

		// TilingWFC 的图块和邻接规则
		// 每条规则: (tile1, orientation1, tile2, orientation2, dirMask)
		// dirMask: DIR_UP|DIR_LEFT|DIR_RIGHT|DIR_DOWN 的位组合
		std::vector<Tile<unsigned>> mTiles;
		std::vector<std::tuple<unsigned, unsigned, unsigned, unsigned, unsigned>> mNeighbors;

		// 每个地形允许的邻接集合（用于 UI 显示，从 mNeighbors 推导）
		// mAllowedNeighbors[terrain] = 所有可以相邻的地形集合
		std::vector<std::vector<unsigned>> mAllowedNeighbors;

		// WFC 生成的输出图像
		std::optional<Array2D<unsigned>> mOutput;

		// WFC 选项
		bool mPeriodicOutput = false;
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