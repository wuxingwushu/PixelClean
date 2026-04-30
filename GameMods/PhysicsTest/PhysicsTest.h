#pragma once
#include "../Configuration.h"
#include "../GameMods.h"
#include "../../Character/UVDynamicDiagram.h"
#include "../../PhysicsBlock/PhysicsWorld.hpp"
#include "../../PhysicsBlock/MapStatic.hpp"
#include <map>

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

	private: // 鼠标点击事件
		void EditorMode(glm::vec2 huoqdedian);
		// 摄像机
		void CameraLeftEvent(bool Click, bool First, glm::vec2 s, glm::vec2 e);
		void CameraRightEvent(bool Click, bool First, glm::vec2 s, glm::vec2 e);
		// 网格形状
		void SquareLeftEvent(PhysicsBlock::PhysicsAngle *Ptr, bool Click, bool First, glm::vec2 s, glm::vec2 e);
		void SquareRightEvent(PhysicsBlock::PhysicsAngle *Ptr, bool Click, bool First, glm::vec2 s, glm::vec2 e);
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

	private:						 // 编辑
		bool EditorModeBool = false; // 编辑模式开关
		int item_object = 0;		 // 类型
		const char *objectName[4] = {u8"圆", u8"点", u8"网格", u8"线"};

	private:														  // 右键菜单
		PhysicsBlock::PhysicsFormwork *RightClickObjectPtr = nullptr; // 右键点击对象指针
		bool ShowRightClickMenu = false;							  // 是否显示右键菜单
		void RenderRightClickMenu();								  // 右键菜单

	private:																 // 网格编辑模式
		PhysicsBlock::PhysicsShape *GridEditShape = nullptr;				 // 网格编辑形状对象
		bool GridEditSavedPhysicsSwitch = true;								 // 是否保存物理世界
		std::map<std::pair<int, int>, PhysicsBlock::GridBlock> GridEditData; // 网格编辑数据
		glm::ivec2 GridEditOrigin = {0, 0};									 // 网格编辑原点
		enum class GridEditBrush
		{
			Draw,
			Erase
		} GridEditBrushType = GridEditBrush::Draw;										   // 网格编辑画笔类型
		bool GridEditCollision = true;													   // 是否开启碰撞检测
		bool GridEditEntity = true;														   // 是否开启实体检测
		FLOAT_ GridEditMass = 1.0f;														   // 网格编辑质量
		FLOAT_ GridEditFriction = 0.2f;													   // 网格编辑摩擦力
		glm::ivec2 GridEditCellRightClick = {-1, -1};									   // 网格编辑单元格右键点击位置
		bool ShowGridCellMenu = false;													   // 是否显示网格单元格菜单
		void RenderGridEditUI();														   // 网格编辑界面
		void RenderGridCellMenu();														   // 网格单元格菜单
		void GridEditPaint(glm::vec2 worldPos);											   // 网格编辑绘制
		glm::ivec2 WorldToGridCell(PhysicsBlock::PhysicsShape *shape, glm::vec2 worldPos); // 世界坐标转换为网格单元格坐标
		void EnterGridEdit(PhysicsBlock::PhysicsShape *shape);							   // 进入网格编辑模式
		void ExitGridEdit();															   // 退出网格编辑模式
	};

}