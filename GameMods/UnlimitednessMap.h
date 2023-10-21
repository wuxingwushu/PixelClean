#pragma once
#include "Configuration.h"
#include "GameMods.h"
#include "../Labyrinth/Dungeon.h"

namespace GAME {

	class UnlimitednessMap : public GameMods, Configuration
	{
	public:
		UnlimitednessMap(Configuration wConfiguration);
		~UnlimitednessMap();

		//鼠标移动事件
		virtual void MouseMove(double xpos, double ypos);

		//鼠标移动事件
		virtual void MouseRoller(int z);

		//键盘事件
		virtual void KeyDown(CAMERA_MOVE moveDirection);

		//获取 CommandBuffer
		virtual void GameCommandBuffers(unsigned int Format_i);

		//游戏循环
		virtual void GameLoop(unsigned int mCurrentFrame);

		//录制 CommandBuffer
		virtual void GameRecordCommandBuffers();
		
		Dungeon* mDungeon = nullptr;//无限地图
	};

}
