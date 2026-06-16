#pragma once11
#include "../Configuration.h"
#include "../GameMods.h"
#include "Dungeon.h"
#include "../../PhysicsBlock/PhysicsAuxiliaryVision.hpp" // PhysicsDebugView 控制器（按值持有，需完整定义）
#include "../../Arms/Arms.h"          // mArms
#include "../../PhysicsBlock/PhysicsWorld.hpp" // mSquarePhysics

namespace GAME {

	class UnlimitednessMapMods : public GameMods, Configuration
	{
	public:
		UnlimitednessMapMods(Configuration wConfiguration);
		~UnlimitednessMapMods();

		//鼠标移动事件
		virtual void MouseMove(double xpos, double ypos);

		//鼠标滚轮事件
		virtual void MouseRoller(int z);

		//鼠标按键事件
		virtual void MouseButton(MouseBtn button, InputState State);

		//键盘事件
		virtual void KeyDown(GameKeyEnum moveDirection);

		//获取 CommandBuffer
		virtual void GameCommandBuffers(unsigned int Format_i);

		//游戏循环
		virtual void GameLoop(unsigned int mCurrentFrame);

		//界面
		virtual void GameUI();

		//录制 CommandBuffer
		virtual void GameRecordCommandBuffers();

		//游戏停止界面循环
		virtual void GameStopInterfaceLoop(unsigned int mCurrentFrame);

		//游戏 TCP事件
		virtual void GameTCPLoop();

	private:
		PhysicsBlock::PhysicsWorld* mSquarePhysics = nullptr;  // 模式私有物理世界
		Arms* mArms = nullptr;                                 // 模式私有武器系统

		Dungeon* mDungeon = nullptr;//无限地图

		bool mLeftMouseDown = false;
		bool mRightMouseDown = false;
		int mWinWidth = 0;
		int mWinHeight = 0;

		PhysicsBlock::PhysicsDebugView mPhysicsDebug; // 物理辅助显示控制器（地图轮廓 + 物理世界 + UI）
	};

}
