#pragma once
#include "../Configuration.h"
#include "../GameMods.h"
#include "../../Character/UVDynamicDiagram.h"
#include "../../PhysicsBlock/PhysicsWorld.hpp"
#include "../../PhysicsBlock/MapStatic.hpp"

namespace GAME {

	class PhysicsTest : public GameMods, Configuration
	{
	public:
		PhysicsTest(Configuration wConfiguration);
		~PhysicsTest();

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
		void ShowStop(glm::dvec2 pos) {
			mAuxiliaryVision->AddStaticLine({ pos.x,pos.y,0 }, { pos.x + 1,pos.y,0 }, { 1,0,0,1 });
			mAuxiliaryVision->AddStaticLine({ pos.x,pos.y,0 }, { pos.x,pos.y + 1,0 }, { 1,0,0,1 });
			mAuxiliaryVision->AddStaticLine({ pos.x,pos.y + 1,0 }, { pos.x + 1,pos.y + 1,0 }, { 1,0,0,1 });
			mAuxiliaryVision->AddStaticLine({ pos.x + 1,pos.y,0 }, { pos.x + 1,pos.y + 1,0 }, { 1,0,0,1 });
		}

		PhysicsBlock::PhysicsWorld* mPhysicsWorld;
		PhysicsBlock::MapStatic* mMapStatic;
	};

}