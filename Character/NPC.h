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

	private:
		int fang = 1;
		int mRange = 160;
		GamePlayer* mNPC = nullptr;
		Labyrinth* mLabyrinth = nullptr;


		int hsuldad = 0;
		std::vector<AStarVec2> LPath;

		AStar* mAStar = nullptr;
	};

}