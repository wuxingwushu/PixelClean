#include "NPC.h"
#include "../Tool/Tool.h"

namespace GAME {

	bool AStarGetWall(int x, int y, void* P) {
		Labyrinth* LLabyrinth = (Labyrinth*)P;
		return LLabyrinth->GetPixelWallNumber(x, y) <= 0;
	}

	NPC::NPC(GamePlayer* npc, Labyrinth* Labyrinth)
	{
		mNPC = npc;
		mLabyrinth = Labyrinth;
		mNPC->GetObjectCollision()->SetAngle(0.01f);
		mAStar = new AStar(mRange, 5000);
		mAStar->SetObstaclesCallback(AStarGetWall, mLabyrinth);
	}

	NPC::~NPC()
	{
		delete mNPC;
		while (!mAStar->GetPathfindingCompleted()) {//等待多线程任务结束
			std::cout << "~NPC() AStar 等待线程结束" << std::endl;
		}
		delete mAStar;
	}

	void NPC::SetNPC(int x, int y, float angle) {
		mNPC->GetObjectCollision()->SetPos({ x, y });
		mNPC->GetObjectCollision()->SetAngle(angle);
		mNPC->setGamePlayerMatrix(3, true);
	}

	void NPC::Event(int Frame, float time) {
		mTime += time;
		mNPC->UpData();

		glm::vec2 pos = mNPC->GetObjectCollision()->GetPos();

		//玩家相对NPC位置角度
		float wanjiaAngle = SquarePhysics::EdgeVecToCosAngleFloat(glm::vec2{Global::GamePlayerX - pos.x, Global::GamePlayerY - pos.y});

		//射线检测
		SquarePhysics::CollisionInfo LInfo = mLabyrinth->mFixedSizeTerrain->RadialCollisionDetection(pos, { Global::GamePlayerX, Global::GamePlayerY });
		if ((fabs(wanjiaAngle - mNPC->GetObjectCollision()->GetAngleFloat()) < 0.785f) && !LInfo.Collision) {//在视野范围内，且可以看到玩家
			mSuspicious = true;
			mSuspiciousPos = { int(LInfo.Pos.x), int(LInfo.Pos.y) };
		}

		


		if (!LInfo.Collision && //看见玩家
			(fabs(wanjiaAngle - mNPC->GetObjectCollision()->GetAngleFloat()) < 0.785f) && //玩家在视野范围内
			mAStar->GetPathfindingCompleted() && //寻路可以用调用（防止反复调用）
			LPath.size() <= 20 && //路径小于多少时
			mTime >= mPathfindingCycle && //时间间隔
			fabs(Global::GamePlayerX - pos.x) < mRange && fabs(Global::GamePlayerY - pos.y) < mRange//玩家在范围内
			) {
			//std::cout << "开始寻路" << std::endl;
			LPath.clear();
			TOOL::mThreadPool->enqueue(&AStar::FindPath, mAStar, AStarVec2{ int(pos.x), int(pos.y) }, AStarVec2{ Global::GamePlayerX, Global::GamePlayerY }, &LPath);
			hsuldad = 0;
			mTime = 0;
			return;
		}


		if (mAStar->GetPathfindingCompleted() && LPath.size() > 20) {//沿着路径前进
			glm::vec2 yiPOS = { (LPath[LPath.size() - 2].x - LPath[LPath.size() - 1].x) , (LPath[LPath.size() - 2].y - LPath[LPath.size() - 1].y) };
			float LAngle = SquarePhysics::EdgeVecToCosAngleFloat(yiPOS);
			yiPOS = pos + (yiPOS * 0.1f);
			hsuldad++;
			//std::cout << "yiPOS：" << yiPOS.x << " - " << yiPOS.y << std::endl;
			//std::cout << "LPath：" << LPath[LPath.size() - 2].x << " - " << LPath[LPath.size() - 2].y << std::endl;
			if (hsuldad >= 10) {
				hsuldad = 0;
				LPath.pop_back();
			}
			//mNPC->mObjectCollision->SetForce(yiPOS * 100.0f);
			mNPC->GetObjectCollision()->SetAngle(wanjiaAngle);
			mNPC->GetObjectCollision()->SetPos(yiPOS);
			mNPC->GetObjectCollision()->SetSpeed(0, 0);
		}
		else if (mSuspicious && mAStar->GetPathfindingCompleted()) {//前往可疑位置
			mSuspicious = false;
			//std::cout << "可疑位置" << std::endl;
			LPath.clear();
			TOOL::mThreadPool->enqueue(&AStar::FindPath, mAStar, AStarVec2{ int(pos.x), int(pos.y) }, mSuspiciousPos, &LPath);
			hsuldad = 0;
		}
		else
		{
			LPath.clear();
			if (mLabyrinth->GetPixelWallNumber(pos.x + fang, pos.y) <= 0) {
				mNPC->GetObjectCollision()->SetPos({ pos.x + (fang * 0.1f), pos.y });
				if (fang > 0) {
					mNPC->GetObjectCollision()->SetAngle(0.0f);
				}
				else
				{
					mNPC->GetObjectCollision()->SetAngle(3.14f);
				}
				//mNPC->mObjectCollision->SetPos({ fang * 100 , 0 });
			}
			else {
				fang = -fang;
			}
		}

		//更新模型位置
		mNPC->setGamePlayerMatrix(Frame);
	}

}