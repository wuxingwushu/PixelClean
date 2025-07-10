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

		virtual void GameUI();

	private:
		void ShowStaticSquare(glm::dvec2 pos, double angle = 0, glm::vec4 color = {1,0,0,1});
		void ShowSquare(glm::dvec2 pos, double angle = 0, glm::vec4 color = {1,0,0,1});

		bool PhysicsSwitch = true;// 物理世界的开关
		PhysicsBlock::PhysicsWorld* mPhysicsWorld = nullptr;// 物理世界
		PhysicsBlock::MapStatic* mMapStatic;// 地图

		PhysicsBlock::PhysicsFormwork* PhysicsFormworkPtr = nullptr;// 选择的物理对象

		// 物理信息辅助视觉
		bool PhysicsAssistantInformation = false; // 是否显示物理辅助信息
		bool PhysicsCollisionDrop = true; // 显示物理碰撞点
		bool PhysicsSeparateNormalVector = true; // 显示分离法向量
		bool PhysicsCollisionDropToCenterOfGravity = true; // 碰撞点指向重心
	};

}