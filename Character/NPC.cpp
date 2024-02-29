#include "NPC.h"
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
		mNPC->GetObjectCollision()->SetAngle(0.01f);
		mJPS = new JPS(mRange, 50000);
		mJPS->SetObstaclesCallback(AStarGetWall, wPathfinding);

		//歪门邪道 生成状态机
		NPC_StateMachine NFSM(this);
		NPCFSM = new FSM(boost::sml::sm<NPC_StateMachine, std::tuple<>>(NFSM));
	}

	NPC::~NPC()
	{
		delete NPCFSM;
		delete mNPC;
		while (!mJPS->GetPathfindingCompleted()) {//等待多线程任务结束
			std::cout << "~NPC() AStar 等待线程结束" << std::endl;
		}
		delete mJPS;
	}

	void NPC::SetNPC(int x, int y, float angle) {
		mNPC->GetObjectCollision()->SetPos({ x, y });
		mNPC->GetObjectCollision()->SetAngle(angle);
		mNPC->setGamePlayerMatrix(3, true);
	}


	int NPC::GetSensoryMessages() {
		int flags = SensoryMessages_None;
		glm::vec2 pos = mNPC->GetObjectCollision()->GetPos();

		//玩家相对NPC位置角度
		wanjiaAngle = SquarePhysics::EdgeVecToCosAngleFloat(glm::vec2{ Global::GamePlayerX - pos.x, Global::GamePlayerY - pos.y });
		float AngleCha = wanjiaAngle - mNPC->GetObjectCollision()->GetAngleFloat();
		if (AngleCha > 3.14f) {
			AngleCha -= 6.28f;
		}
		if (AngleCha < -3.14f) {
			AngleCha += 6.28f;
		}
		if (fabs(AngleCha) < 0.785f) {
			flags |= SensoryMessages_VisualField;//在视野范围内

			//射线检测
			SquarePhysics::CollisionInfo LInfo = wPathfinding->RadialCollisionDetection(pos.x, pos.y, Global::GamePlayerX, Global::GamePlayerY);
			if (!LInfo.Collision) {//在视野范围内，且可以看到玩家
				mSuspicious = true;
				mSuspiciousPos = { (int)Global::GamePlayerX, (int)Global::GamePlayerY };
				flags |= SensoryMessages_Visible;
			}
		}

		if (fabs((Global::GamePlayerX - pos.x) < AttackRange) && (fabs(Global::GamePlayerY - pos.y) < AttackRange))//玩家在范围内
		{
			flags |= SensoryMessages_Range;
		}
		return flags;
	}


	void NPC::Pathfinding() {
		glm::vec2 pos = mNPC->GetObjectCollision()->GetPos();

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
		if (mNPC->mPixelQueue->GetNumber() > 0) {//如果受伤
			mTime = 0.0;
			NPCFSM->Get()->process_event(S_Injury{});
			return true;
		}

		int flags = GetSensoryMessages();
		if (flags & SensoryMessages_Visible) {//是否可见
			if (flags & SensoryMessages_Range) {//是否在范围内
				mTime = mPathfindingCycle + 1.0f;
				NPCFSM->Get()->process_event(S_Chasing{});
				return true;
			}
			else {
				NPCFSM->Get()->process_event(S_Attack{});
				return true;
			}
		}

		mNPC->GetObjectCollision()->PlayerTargetAngle(SquarePhysics::EdgeVecToCosAngleFloat(qianjinfang));//旋转角度
		if (mTime > 1.0f) {
			NPCFSM->Get()->process_event(S_Patrol{});
		}
		return true;
	}

	//巡逻
	bool NPC::Patrol() {
		int flags = GetSensoryMessages();
		if (!(flags & SensoryMessages_Visible) && mNPC->mPixelQueue->GetNumber() > 0) {//如果受伤
			mTime = 0.0;
			NPCFSM->Get()->process_event(S_Injury{});
			return true;
		}
		if (flags & SensoryMessages_Visible) {//是否可见
			if (flags & SensoryMessages_Range) {//是否在范围内
				NPCFSM->Get()->process_event(S_Attack{});
				return true;
			}
			else {
				mTime = mPathfindingCycle + 1.0f;
				NPCFSM->Get()->process_event(S_Chasing{});
				return true;
			}
		}
		
		glm::vec2 pos = mNPC->GetObjectCollision()->GetPos();
		glm::vec2 Lpos = pos + qianjinfang;
		if (wPathfinding->GetPixelWallNumber(Lpos.x + wPathfinding->PathfindingDecoratorDeviationX, Lpos.y + wPathfinding->PathfindingDecoratorDeviationY)) {
			//mNPC->GetObjectCollision()->SetPos(pos + (qianjinfang * 0.1f));
			float YAngle = SquarePhysics::EdgeVecToCosAngleFloat(qianjinfang);
			mNPC->GetObjectCollision()->ExpectSpeed(100, YAngle, FPSTime);
			mNPC->GetObjectCollision()->PlayerTargetAngle(YAngle);
		}
		else {
			switch (rand()%4)
			{
			case 0:
				qianjinfang = { 1, 0 };
				break;
			case 1:
				qianjinfang = { -1, 0 };
				break;
			case 2:
				qianjinfang = { 0, 1 };
				break;
			case 3:
				qianjinfang = { 0, -1 };
				break;
			default:
				break;
			}
			mTime = 0;
			NPCFSM->Get()->process_event(S_Standby{});
		}
		return true;
	}

	//攻击
	bool NPC::Attack() {
		int flags = GetSensoryMessages();
		if ((flags & SensoryMessages_Range) && (flags & SensoryMessages_Visible)) {
			mNPC->GetObjectCollision()->PlayerTargetAngle(wanjiaAngle);
			if (mTime > 0.5f) {
				mTime = 0.0;
				glm::vec2 pos = mNPC->GetObjectCollision()->GetPos();
				pos += SquarePhysics::vec2angle(glm::vec2{ 9.0f, 0.0f }, mNPC->GetObjectCollision()->GetAngleFloat());
				wArms->ShootBullets(pos.x, pos.y, mNPC->GetObjectCollision()->GetAngleFloat(), 500, 0);
			}
		}
		else {
			mTime = mPathfindingCycle + 1.0f;
			NPCFSM->Get()->process_event(S_Chasing{});
		}


		return true;
	}

	//受伤
	bool NPC::Injury() {
		GetSensoryMessages();//感官消息
		mNPC->GetObjectCollision()->PlayerTargetAngle(wanjiaAngle);//旋转角度
		if (mTime > 2.0f) {
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
		int flags = GetSensoryMessages();
		if ((flags & SensoryMessages_Range) && (flags & SensoryMessages_Visible)) {
			NPCFSM->Get()->process_event(S_Attack{});
			return true;
		}

		if (!(flags & SensoryMessages_Visible) && (mNPC->mPixelQueue->GetNumber() > 0)) {//如果受伤
			mTime = 0.0;
			NPCFSM->Get()->process_event(S_Injury{});
			return true;
		}
		

		if (mJPS->GetPathfindingCompleted()) {
			glm::vec2 pos = mNPC->GetObjectCollision()->GetPos();

			if (mSuspicious && mTime > mPathfindingCycle) {
				mSuspicious = false;
				glm::vec2 zhuipos = { mSuspiciousPos.x - pos.x, mSuspiciousPos.y - pos.y };
				while (fabs(zhuipos.x) > mRange || fabs(zhuipos.y) > mRange)
				{
					zhuipos /= 2.0f;
					mSuspicious = true;
				}
				zhuipos += pos;
				LPath.clear();
				TOOL::mThreadPool->enqueue(&JPS::FindPath, mJPS, JPSVec2{ int(pos.x), int(pos.y) }, JPSVec2{ int(zhuipos.x), int(zhuipos.y) }, &LPath,
					JPSVec2{ wPathfinding->PathfindingDecoratorDeviationX, wPathfinding->PathfindingDecoratorDeviationY }
				);
				hsuldad = 0;
				mTime = 0;
				return true;
			}
			
			if (LPath.size() > 0) {//沿着路径前进
				//glm::vec2 yiPOS = { (LPath[LPath.size() - 2].x - LPath[LPath.size() - 1].x) , (LPath[LPath.size() - 2].y - LPath[LPath.size() - 1].y) };
				//yiPOS = pos + (yiPOS * 0.1f);
				//hsuldad++;
				//if (hsuldad >= 10) {
				//	hsuldad = 0;
				//	LPath.pop_back();
				//}

				////玩家相对NPC位置角度
				//float YAngle = SquarePhysics::EdgeVecToCosAngleFloat(glm::vec2{ mSuspiciousPos.x - pos.x, mSuspiciousPos.y - pos.y });

				////mNPC->mObjectCollision->SetForce(yiPOS * 100.0f);
				//mNPC->GetObjectCollision()->SetAngle(YAngle);
				//mNPC->GetObjectCollision()->SetPos(yiPOS);
				//mNPC->GetObjectCollision()->SetSpeed(0, 0);
				JPSVec2 yiPOS = LPath.back();
				JPSVec2 DeviationPOS = yiPOS - JPSVec2{ int(pos.x), int(pos.y)};
				if ((fabs(DeviationPOS.x) < 9) && (fabs(DeviationPOS.y) < 9)) {
					if (LPath.size() > 1) {
						LPath.pop_back();
						yiPOS = LPath.back();
					}
					else {
						LPath.clear();
						if (mSuspicious) {
							mSuspicious = false;
							glm::vec2 zhuipos = { mSuspiciousPos.x - pos.x, mSuspiciousPos.y - pos.y };
							while (fabs(zhuipos.x) > mRange || fabs(zhuipos.y) > mRange)
							{
								zhuipos /= 2.0f;
								mSuspicious = true;
							}
							zhuipos += pos;
							TOOL::mThreadPool->enqueue(&JPS::FindPath, mJPS, JPSVec2{ int(pos.x), int(pos.y) }, JPSVec2{ int(zhuipos.x), int(zhuipos.y) }, &LPath,
								JPSVec2{ wPathfinding->PathfindingDecoratorDeviationX, wPathfinding->PathfindingDecoratorDeviationY }
							);
							hsuldad = 0;
							mTime = 0;
							return true;
						}
					}
				}
				float YAngle = SquarePhysics::EdgeVecToCosAngleFloat(glm::vec2{ yiPOS.x - pos.x, yiPOS.y - pos.y });
				mNPC->GetObjectCollision()->PlayerTargetAngle(YAngle);
				mNPC->GetObjectCollision()->ExpectSpeed(200, YAngle, FPSTime);
				//mNPC->GetObjectCollision()->SufferForce(YAngle, 100);
			}
			else {
				if (!(flags & SensoryMessages_Visible))NPCFSM->Get()->process_event(S_Patrol{});
			}
		}
		return true;
	}
	



	void NPC::Event(int Frame, float time) {
		mTime += time;
		FPSTime = time;
		
		//NPCFSM->Get()->process_event(S_Event{});

		//更新NPC损伤
		mNPC->UpData();
		//更新模型位置
		mNPC->setGamePlayerMatrix(time, Frame);
	}

}