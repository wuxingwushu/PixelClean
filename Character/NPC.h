#pragma once
#include "GamePlayer.h"
#include "../Labyrinth/Labyrinth.h"
#include "../Tool/ContinuousData.h"
#include "../Tool/AStar.h"

namespace GAME {

	class NPC
	{
	public:
		NPC(GamePlayer* npc, Labyrinth* Labyrinth);
		~NPC();

		void Event(int Frame, float time);

		[[nodiscard]] VkCommandBuffer getCommandBuffer(int i) {
			return mNPC->getCommandBuffer(i);
		}

		bool GetDeathInBattle() {
			return mNPC->GetDeathInBattle();
		}

		void InitCommandBuffer() {
			mNPC->InitCommandBuffer();
		}

	private:
		int fang = 1;
		int mRange = 160;//寻路范围
		GamePlayer* mNPC = nullptr;//玩家模型
		Labyrinth* mLabyrinth = nullptr;//地图，用于寻路

		
		int hsuldad = 0;
		float mTime = 0.0f;//当前间隔多久
		const float mPathfindingCycle = 2.0f;//寻路周期
		std::vector<AStarVec2> LPath;//寻路路径

		bool mSuspicious = false;//是否有可疑位置
		AStarVec2 mSuspiciousPos{};//可疑位置

		AStar* mAStar = nullptr;//A星寻路
	};

}