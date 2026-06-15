#pragma once
#include "GamePlayer.h"
#include "../GameMods/PathfindingDecorator.h"
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

		// 获取 NPC 对应的 GamePlayer（用于范围伤害判定）
		GamePlayer* GetGamePlayer() { return mNPC; }

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
		int AttackRange = 90;       // 攻击范围（世界坐标距离）
		int ChaseRange = 250;       // 追击触发范围
		int mRange = 300;//寻路范围
		GamePlayer* mNPC = nullptr;//玩家模型
		PathfindingDecorator* wPathfinding = nullptr;//地图，用于寻路
		Arms* wArms = nullptr;//武器

		int hsuldad = 0;
		float FPSTime = 0;
		float mTime = 0.0f;//当前间隔多久
		const float mPathfindingCycle = 1.5f;//寻路最小周期
		std::vector<JPSVec2> LPath;//寻路路径

		float wanjiaAngle = 0.0f;//NPC到玩家的角度
		bool mSuspicious = false;//是否有可疑位置
		JPSVec2 mSuspiciousPos{};//可疑位置
	float mPlayerDistance = 0.0f; // NPC到玩家的直线距离（每帧更新）

	float mShootCooldown = 0.0f;  // 射击冷却
	const float mShootInterval = 0.8f; // 射击间隔

	// （方案E：mMoveForce 已废弃，移动参数统一由 MovementComponent::MovementConfig 管理）

		JPS* mJPS = nullptr;//A星寻路
	};

}