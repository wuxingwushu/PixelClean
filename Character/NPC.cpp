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
		mNPC->mObjectCollision->SetAngle(0.01f);
		mAStar = new AStar(mRange);
		mAStar->SetObstaclesCallback(AStarGetWall, mLabyrinth);
	}

	NPC::~NPC()
	{
		delete mNPC;
		delete mAStar;
	}

	void NPC::Event(int Frame, float time) {
		mNPC->UpData();
		glm::vec2 pos = mNPC->mObjectCollision->GetPos();

		static float findPathtime = 0;
		findPathtime += time;

		if (mAStar->GetPathfindingCompleted() && LPath.size() <= 10 && findPathtime >= 1.0f && fabs(Global::GamePlayerX - pos.x) < mRange && fabs(Global::GamePlayerY - pos.y) < mRange) {
			std::cout << "开始寻路" << std::endl;
			LPath.clear();
			TOOL::mThreadPool->enqueue(&AStar::findPath, mAStar, AStarVec2{ int(pos.x), int(pos.y) }, AStarVec2{ Global::GamePlayerX, Global::GamePlayerY }, &LPath);
			hsuldad = 0;
			findPathtime = 0;
			//if(LPath.size() == 0) {
			//	std::cout << "没找到路！" << std::endl;
			//}

			/*std::cout << "路径：" << std::endl;
			for (int i = LPath.size() - 1; i >= 0; i--)
				std::cout << "(" << LPath[i].x << ", " << LPath[i].y << ")" << std::endl;*/
			
		}
			
		if (mAStar->GetPathfindingCompleted() && LPath.size() > 10) {
			glm::vec2 yiPOS = { (LPath[LPath.size() - 1].x - LPath[LPath.size() - 2].x) , (LPath[LPath.size() - 1].y - LPath[LPath.size() - 2].y) };
			yiPOS = pos + (yiPOS * -0.1f);
			hsuldad++;
			//std::cout << "yiPOS：" << yiPOS.x << " - " << yiPOS.y << std::endl;
			//std::cout << "LPath：" << LPath[LPath.size() - 2].x << " - " << LPath[LPath.size() - 2].y << std::endl;
			if (hsuldad >= 10) {
				hsuldad = 0;
				LPath.pop_back();
			}
			//mNPC->mObjectCollision->SetForce(yiPOS * 100.0f);
			mNPC->mObjectCollision->SetPos(yiPOS);
			mNPC->mObjectCollision->SetSpeed(0, 0);
		}
		else
		{
			LPath.clear();
			if (mLabyrinth->GetPixelWallNumber(pos.x + fang, pos.y) <= 0) {
				mNPC->mObjectCollision->SetPos({ pos.x + (fang * 0.1f), pos.y });
				//mNPC->mObjectCollision->SetForce({ fang * 100 , 0 });
			}
			else {
				fang = -fang;
			}
		}
		mNPC->setGamePlayerMatrix(glm::rotate(
			glm::translate(glm::mat4(1.0f), glm::vec3(mNPC->mObjectCollision->GetPosX(), mNPC->mObjectCollision->GetPosY(), 0.0f)),
			glm::radians(mNPC->mObjectCollision->GetAngleFloat() * 180.0f / 3.14f),
			glm::vec3(0.0f, 0.0f, 1.0f)
			),
			Frame
		);
	}

}