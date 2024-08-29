#pragma once
#include "../Configuration.h"
#include "../GameMods.h"
#include "Labyrinth.h"
#include "../../Character/UVDynamicDiagram.h"

namespace GAME {

	class MazeMods : public GameMods, Configuration
	{
	public:
		MazeMods(Configuration wConfiguration);
		~MazeMods();

		//鼠标移动事件
		virtual void MouseMove(double xpos, double ypos);

		//鼠标移动事件
		virtual void MouseRoller(int z);

		//键盘事件
		virtual void KeyDown(GameKeyEnum moveDirection);

		//获取 CommandBuffer
		virtual void GameCommandBuffers(unsigned int Format_i);

		//游戏循环
		virtual void GameLoop(unsigned int mCurrentFrame);

		//录制 CommandBuffer
		virtual void GameRecordCommandBuffers();

		//游戏停止界面循环
		virtual void GameStopInterfaceLoop(unsigned int mCurrentFrame);

		//游戏 TCP事件
		virtual void GameTCPLoop();

	private:
		Labyrinth* mLabyrinth = nullptr;//迷宫
		UVDynamicDiagram* mUVDynamicDiagram = nullptr;
	};

}