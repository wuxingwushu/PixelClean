#pragma once
#include "../Configuration.h"
#include "../GameMods.h"
#include "../../Character/UVDynamicDiagram.h"
#include "../../PhysicsBlock/PhysicsWorld.hpp"
#include "../../PhysicsBlock/MapStatic.hpp"

namespace GAME
{

	class PhysicsTest : public GameMods, Configuration
	{
	public:
		PhysicsTest(Configuration wConfiguration);
		~PhysicsTest();

		// 鼠标移动事件
		virtual void MouseMove(double xpos, double ypos);

		// 鼠标移动事件
		virtual void MouseRoller(int z);

		// 键盘事件
		virtual void KeyDown(GameKeyEnum moveDirection);

		// 获取 CommandBuffer
		virtual void GameCommandBuffers(unsigned int Format_i);

		// 游戏循环
		virtual void GameLoop(unsigned int mCurrentFrame);

		// 录制 CommandBuffer
		virtual void GameRecordCommandBuffers();

		// 游戏停止界面循环
		virtual void GameStopInterfaceLoop(unsigned int mCurrentFrame);

		// 游戏 TCP事件
		virtual void GameTCPLoop();

		virtual void GameUI();

	private:// 鼠标点击事件
		void EditorMode(glm::vec2 huoqdedian);
		// 摄像机
		void CameraLeftEvent(bool Click, bool First, glm::vec2 s, glm::vec2 e);
		void CameraRightEvent(bool Click, bool First, glm::vec2 s, glm::vec2 e);
		// 网格形状
		void SquareLeftEvent(PhysicsBlock::PhysicsShape *Ptr, bool Click, bool First, glm::vec2 s, glm::vec2 e);
		void SquareRightEvent(PhysicsBlock::PhysicsShape *Ptr, bool Click, bool First, glm::vec2 s, glm::vec2 e);
		// 圆
		void CircleLeftEvent(PhysicsBlock::PhysicsCircle *Ptr, bool Click, bool First, glm::vec2 s, glm::vec2 e);
		void CircleRightEvent(PhysicsBlock::PhysicsCircle *Ptr, bool Click, bool First, glm::vec2 s, glm::vec2 e);
		// 点
		void ParticleLeftEvent(PhysicsBlock::PhysicsParticle *Ptr, bool Click, bool First, glm::vec2 s, glm::vec2 e);
		void ParticleRightEvent(PhysicsBlock::PhysicsParticle *Ptr, bool Click, bool First, glm::vec2 s, glm::vec2 e);
		// 创建
		void FoundLeftEvent(bool Click, bool First, glm::vec2 s, glm::vec2 e);

	private: // 辅助绘制
		void ShowStaticSquare(glm::dvec2 pos, double angle = 0, glm::vec4 color = {1, 0, 0, 1});
		void ShowSquare(glm::dvec2 pos, double angle = 0, glm::vec4 color = {1, 0, 0, 1});

	private:
		bool PhysicsSwitch = true;							 // 物理世界的开关
		PhysicsBlock::PhysicsWorld *mPhysicsWorld = nullptr; // 物理世界
		PhysicsBlock::MapStatic *mMapStatic;				 // 地图

		PhysicsBlock::PhysicsFormwork *PhysicsFormworkPtr = nullptr; // 选择的物理对象

		// 物理信息辅助视觉
		bool PhysicsAssistantInformation = false; // 是否显示物理辅助信息

	private:// 编辑
		bool EditorModeBool = false; // 编辑模式开关
		int item_object = 0; // 类型
		const char *objectName[4] = {u8"圆", u8"点", u8"网格", u8"线"};
	};

}