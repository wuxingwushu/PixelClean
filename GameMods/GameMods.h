#pragma once
class GameMods
{
public:
	//鼠标移动事件
	virtual void MouseMove(double xpos, double ypos) = 0;

	//鼠标移动事件
	virtual void MouseRoller(int z) = 0;

	//键盘事件
	virtual void KeyDown(CAMERA_MOVE moveDirection) = 0;

	//获取 CommandBuffer
	virtual void GameCommandBuffers(unsigned int Format_i) = 0;

	//游戏循环
	virtual void GameLoop(unsigned int mCurrentFrame) = 0;

	//录制 CommandBuffer
	virtual void GameRecordCommandBuffers() = 0;
};
