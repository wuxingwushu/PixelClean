#pragma once
#include "../Configuration.h"
#include "../GameMods.h"
#include "FixedMaze.h"
#include "../../Character/UVDynamicDiagram.h"
#include "../../Arms/AttackMode.h"
#include "../../PhysicsBlock/PhysicsAuxiliaryVision.hpp" // PhysicsDebugView 控制器（按值持有，需完整定义）
#include "../../Arms/Arms.h"          // mArms
#include "../../PhysicsBlock/PhysicsWorld.hpp" // mSquarePhysics

namespace GAME {

	class TankTrouble : public GameMods, Configuration
	{
	public:
		TankTrouble(Configuration wConfiguration);
		~TankTrouble();

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

		// 把 AttackType 同步到 Arms 的当前武器配置（反弹/爆炸/射速）
		void UpdateWeaponMode();

		// 检查并处理玩家死亡/重生、NPC 死亡计分
		void UpdateGameRules(float time);

		FixedMaze* mLabyrinth = nullptr;//迷宫
		UVDynamicDiagram* mUVDynamicDiagram = nullptr;

		bool mLeftMouseDown = false;
		bool mRightMouseDown = false;
		int mWinWidth = 0;
		int mWinHeight = 0;

		PhysicsBlock::PhysicsDebugView mPhysicsDebug; // 物理辅助显示控制器（地图轮廓 + 物理世界 + UI）

		// ===== 游戏规则状态（胜负判定 + 重生）=====
		int mPlayerKills = 0;            // 玩家击杀数（杀 NPC 计数）
		int mPlayerDeaths = 0;           // 玩家死亡数
		int mKillTarget = 5;             // 获胜所需击杀数
		bool mGameEnded = false;         // 游戏是否结束（达成目标）
		bool mPlayerWon = false;         // 玩家是否获胜
		float mRespawnTimer = 0.0f;      // 重生倒计时（>0 表示等待重生）
		bool mPlayerDying = false;       // 玩家正处于死亡-重生流程中
		const float mRespawnDelay = 2.0f;// 重生延迟（秒）
	};

}