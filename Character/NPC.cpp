#include "NPC.h"
#include "../DebugLog.h"
#include "../Tool/Tool.h"
#include <functional>
#include "../GlobalVariable.h"

namespace GAME {


	//事件
	struct S_Event {};

	//待机
	class C_Standby; struct S_Standby {};
	//巡逻
	class C_Patrol; struct S_Patrol {};
	//追击
	class C_Chasing; struct S_Chasing {};
	//攻击
	class C_Attack; struct S_Attack {};
	//受伤
	class C_Injury; struct S_Injury {};
	//逃跑
	class C_Escape; struct S_Escape {};

	namespace {
		struct NPC_StateMachine {
			NPC* mNpc = nullptr;
			NPC_StateMachine(NPC* npc) :mNpc(npc) {}

			auto operator()() const {
				return boost::sml::make_transition_table(
					//待机
					*boost::sml::state<C_Standby> +boost::sml::event<S_Event>[std::bind(&NPC::Standby, mNpc)],
					boost::sml::state<C_Standby> +boost::sml::on_entry<boost::sml::_> / [] { std::cout << "进入 待机 状态!" << std::endl; },
					//boost::sml::state<C_Standby> +boost::sml::on_exit<boost::sml::_> / [] { std::cout << "结束 待机!" << std::endl; },
					boost::sml::state<C_Standby> +boost::sml::event<S_Patrol> = boost::sml::state<C_Patrol>,//巡逻
					boost::sml::state<C_Standby> +boost::sml::event<S_Chasing> = boost::sml::state<C_Chasing>,//追击
					boost::sml::state<C_Standby> +boost::sml::event<S_Attack> = boost::sml::state<C_Attack>,//攻击
					boost::sml::state<C_Standby> +boost::sml::event<S_Injury> = boost::sml::state<C_Injury>,//受伤
					//巡逻
					boost::sml::state<C_Patrol> +boost::sml::event<S_Event>[std::bind(&NPC::Patrol, mNpc)],
					boost::sml::state<C_Patrol> +boost::sml::on_entry<boost::sml::_> / [] { std::cout << "进入 巡逻 状态!" << std::endl; },
					boost::sml::state<C_Patrol> +boost::sml::event<S_Standby> = boost::sml::state<C_Standby>,//待机
					boost::sml::state<C_Patrol> +boost::sml::event<S_Attack> = boost::sml::state<C_Attack>,//攻击
					boost::sml::state<C_Patrol> +boost::sml::event<S_Chasing> = boost::sml::state<C_Chasing>,//追击
					boost::sml::state<C_Patrol> +boost::sml::event<S_Injury> = boost::sml::state<C_Injury>,//受伤
					//追击
					boost::sml::state<C_Chasing> +boost::sml::event<S_Event>[std::bind(&NPC::GoToLocation, mNpc)],
					boost::sml::state<C_Chasing> +boost::sml::on_entry<boost::sml::_> / [] { std::cout << "进入 追击 状态!" << std::endl; },
					boost::sml::state<C_Chasing> +boost::sml::event<S_Patrol> = boost::sml::state<C_Patrol>,//巡逻
					boost::sml::state<C_Chasing> +boost::sml::event<S_Attack> = boost::sml::state<C_Attack>,//攻击
					boost::sml::state<C_Chasing> +boost::sml::event<S_Injury> = boost::sml::state<C_Injury>,//受伤
					//攻击
					boost::sml::state<C_Attack> +boost::sml::event<S_Event>[std::bind(&NPC::Attack, mNpc)],
					boost::sml::state<C_Attack> +boost::sml::on_entry<boost::sml::_> / [] { std::cout << "进入 攻击 状态!" << std::endl; },
					boost::sml::state<C_Attack> +boost::sml::event<S_Patrol> = boost::sml::state<C_Patrol>,//巡逻
					boost::sml::state<C_Attack> +boost::sml::event<S_Chasing> = boost::sml::state<C_Chasing>,//追击
					boost::sml::state<C_Attack> +boost::sml::event<S_Escape> = boost::sml::state<C_Escape>,//逃跑
					//受伤
					boost::sml::state<C_Injury> +boost::sml::event<S_Event>[std::bind(&NPC::Injury, mNpc)],
					boost::sml::state<C_Injury> +boost::sml::on_entry<boost::sml::_> / [] { std::cout << "进入 受伤 状态!" << std::endl; },
					boost::sml::state<C_Injury> +boost::sml::event<S_Chasing> = boost::sml::state<C_Chasing>,//追击
					//逃跑
					boost::sml::state<C_Escape> +boost::sml::event<S_Event>[std::bind(&NPC::Standby, mNpc)],
					boost::sml::state<C_Escape> +boost::sml::on_entry<boost::sml::_> / [] { std::cout << "进入 逃跑 状态!" << std::endl; },
					boost::sml::state<C_Escape> +boost::sml::event<S_Patrol> = boost::sml::state<C_Patrol>//巡逻
				);
			}
		};
	}


	class FSM
	{
	public:
		FSM(boost::sml::sm<NPC_StateMachine, std::tuple<>> LNPCFSM) : NPCFSM(LNPCFSM) {};
		
		boost::sml::sm<NPC_StateMachine, std::tuple<>> NPCFSM;

		auto Get() {
			return &NPCFSM;
		}

		~FSM(){}
	};


	bool AStarGetWall(int x, int y, void* P) {
		PathfindingDecorator* Pathfinding = (PathfindingDecorator*)P;
		return Pathfinding->GetPixelWallNumber(x, y);
	}
	
	NPC::NPC(GamePlayer* npc, PathfindingDecorator* pathfinding, Arms* arms)
	{
		mNPC = npc;
		wPathfinding = pathfinding;
		wArms = arms;
		mNPC->GetObjectCollision()->angle = 0.01f;
		mJPS = new JPS(mRange, 50000);
		mJPS->SetObstaclesCallback(AStarGetWall, wPathfinding);

		// 设置 NPC 专属移动参数（比玩家略慢、转向更平滑）
		MovementComponent* mc = mNPC->GetMovement();
		if (mc) {
			mc->Config().MaxSpeed = 90.0f;   // NPC 最高速度（玩家 120）
			mc->Config().TurnRate = 7.0f;    // NPC 转向稍慢，更"思考"
			mc->Config().RagdollMinTime = 0.4f; // 击飞硬直稍长
		}

		//歪门邪道 生成状态机
		NPC_StateMachine NFSM(this);
		NPCFSM = new FSM(boost::sml::sm<NPC_StateMachine, std::tuple<>>(NFSM));
	}

	NPC::~NPC()
	{
		LOGD("NPC::~NPC() called");
		delete NPCFSM;
		delete mNPC;
		while (!mJPS->GetPathfindingCompleted()) {//等待多线程任务结束
			std::cout << "~NPC() AStar 等待线程结束" << std::endl;
		}
		delete mJPS;
	}

	void NPC::SetNPC(int x, int y, float angle) {
		mNPC->GetObjectCollision()->pos = Vec2_{static_cast<FLOAT_>(x), static_cast<FLOAT_>(y)};
		mNPC->GetObjectCollision()->angle = angle;
		mNPC->setGamePlayerMatrix(3, true);
	}


	int NPC::GetSensoryMessages() {
		int flags = SensoryMessages_None;
		glm::vec2 pos = mNPC->GetObjectCollision()->pos;

		float dx = Global::GamePlayerX - pos.x;
		float dy = Global::GamePlayerY - pos.y;
		mPlayerDistance = std::sqrt(dx * dx + dy * dy);

		//玩家相对NPC位置角度
		wanjiaAngle = PhysicsBlock::EdgeVecToCosAngleFloat(glm::vec2{ dx, dy });
		float AngleCha = wanjiaAngle - mNPC->GetObjectCollision()->angle;
		if (AngleCha > 3.14f) {
			AngleCha -= 6.28f;
		}
		if (AngleCha < -3.14f) {
			AngleCha += 6.28f;
		}
		// 视野锥：扩大到 90°（±1.57 弧度），让 NPC 更容易察觉玩家
		if (fabs(AngleCha) < 1.57f) {
			flags |= SensoryMessages_VisualField;//在视野范围内

			//射线检测
			PhysicsBlock::CollisionInfoI LInfo = wPathfinding->RadialCollisionDetection(pos.x, pos.y, Global::GamePlayerX, Global::GamePlayerY);
			if (!LInfo.Collision) {//在视野范围内，且可以看到玩家
				mSuspicious = true;
				mSuspiciousPos = { (int)Global::GamePlayerX, (int)Global::GamePlayerY };
				flags |= SensoryMessages_Visible;
			}
		}

		// 范围判定：用 mPlayerDistance 与 AttackRange 比较（修复原 fabs((x<range)) 的逻辑错误）
		if (mPlayerDistance < AttackRange) {
			flags |= SensoryMessages_Range;
		}
		return flags;
	}


	void NPC::Pathfinding() {
		glm::vec2 pos = mNPC->GetObjectCollision()->pos;

		if (mJPS->GetPathfindingCompleted() && //寻路可以用调用（防止反复调用）
			LPath.size() <= 20 && //路径小于多少时
			mTime >= mPathfindingCycle && //时间间隔
			fabs(Global::GamePlayerX - pos.x) < AttackRange && fabs(Global::GamePlayerY - pos.y) < AttackRange//玩家在范围内
			) {
			//std::cout << "开始寻路" << std::endl;
			LPath.clear();
			TOOL::mThreadPool->enqueue(&JPS::FindPath, mJPS, JPSVec2{ int(pos.x), int(pos.y) }, JPSVec2{ (int)Global::GamePlayerX, (int)Global::GamePlayerY }, &LPath,
				JPSVec2{ wPathfinding->PathfindingDecoratorDeviationX, wPathfinding->PathfindingDecoratorDeviationY }
			);
			hsuldad = 0;
			mTime = 0;
			return;
		}
	}


	//待机
	bool NPC::Standby() {
		// 不在被击飞态时才恢复受控（击飞态由 MovementComponent 自动切回 Controlled）
		if (mNPC->GetMovement()->GetMode() != MovementMode::Ragdoll) {
			mNPC->GetMovement()->SetMode(MovementMode::Controlled);
		}

		if (mNPC->mPixelQueue->GetNumber() > 0) {//如果受伤
			mTime = 0.0;
			NPCFSM->Get()->process_event(S_Injury{});
			return true;
		}

		int flags = GetSensoryMessages();
		if (flags & SensoryMessages_Visible) {//看到玩家
			if (flags & SensoryMessages_Range) {
				// 近距离 → 攻击
				NPCFSM->Get()->process_event(S_Attack{});
			} else {
				// 远距离 → 追击
				mTime = mPathfindingCycle + 1.0f;
				NPCFSM->Get()->process_event(S_Chasing{});
			}
			return true;
		}

		// 待机时缓慢转向前进方向
		mNPC->GetMovement()->SetLookAngle(PhysicsBlock::EdgeVecToCosAngleFloat(qianjinfang));
		if (mTime > 1.5f) {
			NPCFSM->Get()->process_event(S_Patrol{});
		}
		return true;
	}

	//巡逻
	bool NPC::Patrol() {
		// 不在被击飞态时才恢复受控（击飞态由 MovementComponent 自动切回 Controlled）
		if (mNPC->GetMovement()->GetMode() != MovementMode::Ragdoll) {
			mNPC->GetMovement()->SetMode(MovementMode::Controlled);
		}

		int flags = GetSensoryMessages();
		if (!(flags & SensoryMessages_Visible) && mNPC->mPixelQueue->GetNumber() > 0) {//如果受伤
			mTime = 0.0;
			NPCFSM->Get()->process_event(S_Injury{});
			return true;
		}
		if (flags & SensoryMessages_Visible) {//看到玩家
			if (flags & SensoryMessages_Range) {
				NPCFSM->Get()->process_event(S_Attack{});
			} else {
				mTime = mPathfindingCycle + 1.0f;
				NPCFSM->Get()->process_event(S_Chasing{});
			}
			return true;
		}

		glm::vec2 pos = mNPC->GetObjectCollision()->pos;
		glm::vec2 Lpos = pos + qianjinfang * 2.0f;
		// GetPixelWallNumber 返回 true 表示该格是墙（不可通行）
		if (!wPathfinding->GetPixelWallNumber(Lpos.x + wPathfinding->PathfindingDecoratorDeviationX, Lpos.y + wPathfinding->PathfindingDecoratorDeviationY)) {
			// 前方无墙：设置朝向 + 方向投票
			float YAngle = PhysicsBlock::EdgeVecToCosAngleFloat(qianjinfang);
			mNPC->GetMovement()->SetLookAngle(YAngle);
			mNPC->GetMovement()->SetMoveInput(Vec2_{ qianjinfang.x, qianjinfang.y });
		}
		else {
			// 前方有墙：随机选新方向
			switch (rand()%4)
			{
			case 0: qianjinfang = { 1, 0 }; break;
			case 1: qianjinfang = { -1, 0 }; break;
			case 2: qianjinfang = { 0, 1 }; break;
			case 3: qianjinfang = { 0, -1 }; break;
			default: break;
			}
			mTime = 0;
			NPCFSM->Get()->process_event(S_Standby{});
		}
		return true;
	}

	//攻击
	bool NPC::Attack() {
		// 不在被击飞态时才恢复受控（击飞态由 MovementComponent 自动切回 Controlled）
		if (mNPC->GetMovement()->GetMode() != MovementMode::Ragdoll) {
			mNPC->GetMovement()->SetMode(MovementMode::Controlled);
		}

		int flags = GetSensoryMessages();
		if ((flags & SensoryMessages_Range) && (flags & SensoryMessages_Visible)) {
			// 朝向玩家
			mNPC->GetMovement()->SetLookAngle(wanjiaAngle);

			// 保持一定距离：太近就后退，太远就靠近（保持中距离射击）
			glm::vec2 pos = mNPC->GetObjectCollision()->pos;
			glm::vec2 toPlayer = glm::vec2{ Global::GamePlayerX - pos.x, Global::GamePlayerY - pos.y };
			float dist = mPlayerDistance;
			float idealDist = 50.0f;
			glm::vec2 moveDir = glm::normalize(toPlayer);
			if (dist < idealDist - 10.0f) {
				// 太近，后退
				mNPC->GetMovement()->SetMoveInput(Vec2_{ -moveDir.x, -moveDir.y });
			} else if (dist > idealDist + 10.0f) {
				// 太远，靠近
				mNPC->GetMovement()->SetMoveInput(Vec2_{ moveDir.x, moveDir.y });
			}

			// 射击（带冷却）
			mShootCooldown -= FPSTime;
			if (mShootCooldown <= 0.0f) {
				mShootCooldown = mShootInterval;
				glm::vec2 shootPos = pos + PhysicsBlock::vec2angle(glm::vec2{ 9.0f, 0.0f }, mNPC->GetObjectCollision()->angle);
				// 加入轻微瞄准误差（让 AI 不至于百发百中）
				float aimError = ((rand() % 100) / 100.0f - 0.5f) * 0.15f;
				wArms->ShootBullets(shootPos.x, shootPos.y, mNPC->GetObjectCollision()->angle + aimError, 500, 0);
			}
		}
		else {
			// 失去目标 → 追击
			mTime = mPathfindingCycle + 1.0f;
			NPCFSM->Get()->process_event(S_Chasing{});
		}
		return true;
	}

	//受伤（方案E：硬直态，冻结移动）
	bool NPC::Injury() {
		// 进入硬直：冻结移动，但仍可转向朝向玩家
		mNPC->GetMovement()->SetMode(MovementMode::Frozen);
		mNPC->GetMovement()->SetLookAngle(wanjiaAngle);

		int flags = GetSensoryMessages();
		// 如果看到玩家，立即反击或追击
		if (flags & SensoryMessages_Visible) {
			if (flags & SensoryMessages_Range) {
				NPCFSM->Get()->process_event(S_Attack{});
			} else {
				mTime = mPathfindingCycle + 1.0f;
				NPCFSM->Get()->process_event(S_Chasing{});
			}
			return true;
		}

		// 受伤后短暂硬直，然后追击最后目击位置
		if (mTime > 1.0f) {
			mTime = mPathfindingCycle + 1.0f;
			NPCFSM->Get()->process_event(S_Chasing{});
		}
		return true;
	}

	//逃跑
	bool NPC::Escape() {
		return true;
	}

	//追击
	bool NPC::GoToLocation() {
		// 不在被击飞态时才恢复受控（击飞态由 MovementComponent 自动切回 Controlled）
		if (mNPC->GetMovement()->GetMode() != MovementMode::Ragdoll) {
			mNPC->GetMovement()->SetMode(MovementMode::Controlled);
		}

		int flags = GetSensoryMessages();
		// 看到玩家且在攻击范围内 → 攻击
		if ((flags & SensoryMessages_Range) && (flags & SensoryMessages_Visible)) {
			NPCFSM->Get()->process_event(S_Attack{});
			return true;
		}

		// 看不到玩家但受伤 → 受伤状态
		if (!(flags & SensoryMessages_Visible) && (mNPC->mPixelQueue->GetNumber() > 0)) {
			mTime = 0.0;
			NPCFSM->Get()->process_event(S_Injury{});
			return true;
		}

		if (!mJPS->GetPathfindingCompleted()) return true; // 寻路中，等待

		glm::vec2 pos = mNPC->GetObjectCollision()->pos;

		// === 关键修复：无论是否可疑，只要需要就重新寻路 ===
		// 寻路触发条件：路径为空 OR 到了周期时间 OR 有新可疑位置
		bool needRepath = (LPath.empty()) || (mTime > mPathfindingCycle);

		if (needRepath) {
			JPSVec2 target;
			if (mSuspicious) {
				// 有最后目击位置 → 前往该位置
				target = mSuspiciousPos;
				mSuspicious = false; // 消费掉
			} else if (flags & SensoryMessages_Visible) {
				// 还能看到玩家（但可能不在攻击范围内）→ 直接追玩家
				target = { (int)Global::GamePlayerX, (int)Global::GamePlayerY };
			} else {
				// 完全丢失目标 → 巡逻
				NPCFSM->Get()->process_event(S_Patrol{});
				return true;
			}

			// 距离过远时截断到寻路范围内
			glm::vec2 toTarget = glm::vec2{ target.x - pos.x, target.y - pos.y };
			while (fabs(toTarget.x) > mRange || fabs(toTarget.y) > mRange) {
				toTarget *= 0.5f;
			}
			JPSVec2 clampedTarget = { (int)(pos.x + toTarget.x), (int)(pos.y + toTarget.y) };

			LPath.clear();
			TOOL::mThreadPool->enqueue(&JPS::FindPath, mJPS,
				JPSVec2{ (int)pos.x, (int)pos.y }, clampedTarget, &LPath,
				JPSVec2{ wPathfinding->PathfindingDecoratorDeviationX, wPathfinding->PathfindingDecoratorDeviationY }
			);
			hsuldad = 0;
			mTime = 0;
			return true;
		}

		// === 沿路径前进 ===
		if (LPath.empty()) return true;

		JPSVec2 yiPOS = LPath.back();
		glm::vec2 toWaypoint = glm::vec2{ (float)(yiPOS.x) - pos.x, (float)(yiPOS.y) - pos.y };
		float distToWaypoint = glm::length(toWaypoint);

		// 到达当前路径点 → 移除并取下一个
		if (distToWaypoint < 9.0f) {
			LPath.pop_back();
			if (LPath.empty()) return true;
			yiPOS = LPath.back();
			toWaypoint = glm::vec2{ (float)(yiPOS.x) - pos.x, (float)(yiPOS.y) - pos.y };
			distToWaypoint = glm::length(toWaypoint);
			if (distToWaypoint < 0.001f) return true;
		}

		// 朝向路径点 + 设置移动方向
		float YAngle = PhysicsBlock::EdgeVecToCosAngleFloat(toWaypoint);
		mNPC->GetMovement()->SetLookAngle(YAngle);
		glm::vec2 dir = toWaypoint / distToWaypoint;
		mNPC->GetMovement()->SetMoveInput(Vec2_{ dir.x, dir.y });

		return true;
	}
	



	void NPC::Event(int Frame, float time) {
		mTime += time;
		FPSTime = time;
		
		NPCFSM->Get()->process_event(S_Event{});

		// 方案E：统一施力（FSM 只设意图，这里集中翻译成物理操作）
		mNPC->GetMovement()->Update(FPSTime);

		//更新NPC损伤
		mNPC->UpData();
		//更新模型位置
		mNPC->setGamePlayerMatrix(time, Frame);
	}

}