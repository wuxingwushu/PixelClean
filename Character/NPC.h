#pragma once
#include "GamePlayer.h"
#include "../Labyrinth/PathfindingDecorator.h"
#include "../Tool/ContinuousData.h"
#include "../Tool/JPS.h"
#include "../Tool/sml.hpp"
#include "../Arms/Arms.h"

namespace GAME {
	
	class FSM;


	enum SensoryMessagesFlags_
	{
		SensoryMessages_None			= 0,
		SensoryMessages_VisualField		= 1 << 0,//在视野范围内
		SensoryMessages_Visible			= 1 << 1,//可见
		SensoryMessages_Range			= 1 << 2,//攻击范围内
	};


	class NPC
	{
	public:
		NPC(GamePlayer* npc, PathfindingDecorator* pathfinding, Arms* arms);
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

		evutil_socket_t GetKey() {
			return mNPC->GetKey();
		}

		void SetNPC(int x, int y, float angle);

		//寻路
		void Pathfinding();


		//状态机
		FSM* NPCFSM = nullptr;//歪门邪道

		//感官消息
		int GetSensoryMessages();

		//待机
		bool Standby();
		//巡逻
		bool Patrol();
		//追击
		bool GoToLocation();
		//攻击
		bool Attack();
		//受伤
		bool Injury();
		//逃跑
		bool Escape();

	private:
		glm::vec2 qianjinfang = {1,0};
		int AttackRange = 90;
		int mRange = 300;//寻路范围
		GamePlayer* mNPC = nullptr;//玩家模型
		PathfindingDecorator* wPathfinding = nullptr;//地图，用于寻路
		Arms* wArms = nullptr;//武器

		
		int hsuldad = 0;
		float FPSTime = 0;
		float mTime = 0.0f;//当前间隔多久
		const float mPathfindingCycle = 2.0f;//寻路最小周期
		std::vector<JPSVec2> LPath;//寻路路径

		float wanjiaAngle = 0.0f;//NPC到玩家的角度
		bool mSuspicious = false;//是否有可疑位置
		JPSVec2 mSuspiciousPos{};//可疑位置

		JPS* mJPS = nullptr;//A星寻路
	};

}