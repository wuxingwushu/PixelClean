#pragma once
#include "../Configuration.h"
#include "../GameMods.h"
#include "Labyrinth.h"
#include "../../Character/UVDynamicDiagram.h"
#include "../../NetworkTCP/Replication/ReplicationManager.h"
#include "../../NetworkTCP/Replication/NetworkLayer.h"
#include "MazeReplicationComponents.h"
#include "MazeReplicationEvents.h"
#include <memory>

namespace GAME {

	class MazeMods : public GameMods, Configuration
	{
	public:
		MazeMods(Configuration wConfiguration);
		~MazeMods();

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

		//录制 CommandBuffer
		virtual void GameRecordCommandBuffers();

		//游戏停止界面循环
		virtual void GameStopInterfaceLoop(unsigned int mCurrentFrame);

		//游戏 TCP事件
		virtual void GameTCPLoop();

		bool HasPendingLabyrinthInit() const { return mPendingLabyrinthInit != nullptr; }
		void ProcessPendingLabyrinthInit();

	private:
		Labyrinth* mLabyrinth = nullptr;//迷宫
		bool mLabyrinthVulkanReady = false; // 是否已经完成 Vulkan 初始化
		bool mJustLoadedLabyrinth = false; // 刚完成迷宫加载，需跳过本帧迷雾更新
		UVDynamicDiagram* mUVDynamicDiagram = nullptr;

		bool mLeftMouseDown = false;
		bool mRightMouseDown = false;
		int mWinWidth = 0;
		int mWinHeight = 0;

		// === Replication 系统 ===
		ReplicableObject* mLocalPlayerObj = nullptr;
		PlayerPositionComponent* mPlayerPosComp = nullptr;
		PlayerBrokenComponent* mPlayerBrokenComp = nullptr;
		bool mInitRequested = false;

		// === 延迟加载迷宫（避免网络回调中直接操作 Vulkan 资源） ===
		std::unique_ptr<LabyrinthInitDataEvent> mPendingLabyrinthInit;
	};

}