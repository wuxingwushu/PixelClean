#pragma once

enum GameModsEnum
{
	Maze_ = 0,
	TankTrouble_ = 1,
	Infinite_//作为截至符号
};

static const char* GameModsEnumName[]{
	u8"迷宫",
	u8"坦克动荡",
	u8"无限"
};

enum GameKeyEnum
{
	MOVE_LEFT = 0,
	MOVE_RIGHT,
	MOVE_FRONT,
	MOVE_BACK,

	ESC,
	Key_1,
	Key_2,
};

class GameMods
{
public:
	//鼠标移动事件
	virtual void MouseMove(double xpos, double ypos) = 0;

	//鼠标移动事件
	virtual void MouseRoller(int z) = 0;

	//键盘事件
	virtual void KeyDown(GameKeyEnum moveDirection) = 0;

	//获取 CommandBuffer
	virtual void GameCommandBuffers(unsigned int Format_i) = 0;

	//游戏循环
	virtual void GameLoop(unsigned int mCurrentFrame) = 0;

	//录制 CommandBuffer
	virtual void GameRecordCommandBuffers() = 0;

	//游戏停止界面循环
	virtual void GameStopInterfaceLoop(unsigned int mCurrentFrame) = 0;

	//游戏 TCP事件
	virtual void GameTCPLoop() = 0;
};
