#include "UnlimitednessMapMods.h"
#include "../Opcode/OpcodeFunction.h"
#include "../Physics/DestroyMode.h"

namespace GAME {

	bool DungeonGetWallD(int x, int y, void* P) {
		Dungeon* LLabyrinth = (Dungeon*)P;
		return LLabyrinth->GetPixelWallNumber(x, y);
	}

	UnlimitednessMapMods::UnlimitednessMapMods(Configuration wConfiguration) : Configuration{ wConfiguration }
	{
		mAuxiliaryVision = new VulKan::AuxiliaryVision(mDevice, mPipelineS, 1000);
		mAuxiliaryVision->initUniformManager(
			mSwapChain->getImageCount(),
			mCameraVPMatricesBuffer
		);
		mAuxiliaryVision->RecordingCommandBuffer(mRenderPass, mSwapChain);

		//测试寻路
		JPSPathfinding = new JPS(300, 10000);
		AStarPathfinding = new AStar(300, 10000);

		float mGamePlayerPosX = -160;
		float mGamePlayerPosY = 0;
		mDungeon = new Dungeon(mDevice, 100, 60, mSquarePhysics, mGamePlayerPosX, mGamePlayerPosY);
		mDungeon->initUniformManager(
			mSwapChain->getImageCount(),
			mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods)->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler,
			mTextureLibrary
		);
		mDungeon->RecordingCommandBuffer(mRenderPass, mSwapChain, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods), mPipelineS->GetPipeline(VulKan::PipelineMods::GifMods));

		JPSPathfinding->SetObstaclesCallback(DungeonGetWallD, mDungeon);
		AStarPathfinding->SetObstaclesCallback(DungeonGetWallD, mDungeon);

		mVisualEffect = new VulKan::VisualEffect(mDevice);
		mVisualEffect->initUniformManager(
			mSwapChain->getImageCount(),
			mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods)->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler
		);
		mVisualEffect->RecordingCommandBuffer(mRenderPass, mSwapChain, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods));

		//创建多人玩家
		mCrowd = new Crowd(100, mDevice, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods), mSwapChain, mRenderPass, mSampler, mCameraVPMatricesBuffer, mDungeon);
		mCrowd->SteSquarePhysics(mSquarePhysics);
		mCrowd->SetArms(mArms);


		//创建玩家
		mGamePlayer = new GamePlayer(mDevice, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods), mSwapChain, mRenderPass, mSquarePhysics, mGamePlayerPosX, mGamePlayerPosY);
		mGamePlayer->initUniformManager(
			mDevice,
			mCommandPool,
			mSwapChain->getImageCount(),
			10,
			mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods)->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler
		);
		mGamePlayer->InitCommandBuffer();

		VulKan::AuxiliaryForceData* ALine = mAuxiliaryVision->GetContinuousForce()->New(mGamePlayer->GetObjectCollision()->GetForcePointer());
		ALine->pos = mGamePlayer->GetObjectCollision()->GetPosPointer();
		ALine->Force = mGamePlayer->GetObjectCollision()->GetForcePointer();
		ALine->Color = { 0, 0, 1.0f, 1.0f };
		ALine = mAuxiliaryVision->GetContinuousForce()->New(mGamePlayer->GetObjectCollision()->GetSpeedPointer());
		ALine->pos = mGamePlayer->GetObjectCollision()->GetPosPointer();
		ALine->Force = mGamePlayer->GetObjectCollision()->GetSpeedPointer();
		ALine->Color = { 0, 1.0f, 0, 1.0f };

		//给操作码对象赋值
		Opcode::OpArms = mArms;
		Opcode::OpCrowd = mCrowd;
		Opcode::OpGamePlayer = mGamePlayer;
		Opcode::OpImGuiInterFace = InterFace;

		mCrowd->AddNPC(-208, 60);
	}

	UnlimitednessMapMods::~UnlimitednessMapMods()
	{
		delete JPSPathfinding;
		delete AStarPathfinding;
		delete mAuxiliaryVision;
		delete mDungeon;
		delete mCrowd;
		delete mGamePlayer;
		delete mVisualEffect;
	}

	//鼠标移动事件
	void UnlimitednessMapMods::MouseMove(double xpos, double ypos) {
		//mCamera->onMouseMove(xpos, ypos);
	}

	//鼠标移动事件
	void UnlimitednessMapMods::MouseRoller(int z) {
		m_position.z += z * 10;
		mCamera->update();
	}

	//键盘事件
	void UnlimitednessMapMods::KeyDown(GameKeyEnum moveDirection) {
		const int lidaxiao = 100;
		switch (moveDirection)
		{
		case GameKeyEnum::MOVE_LEFT:
			PlayerForce.x -= lidaxiao;
			break;
		case GameKeyEnum::MOVE_RIGHT:
			PlayerForce.x += lidaxiao;
			break;
		case GameKeyEnum::MOVE_FRONT:
			PlayerForce.y += lidaxiao;
			break;
		case GameKeyEnum::MOVE_BACK:
			PlayerForce.y -= lidaxiao;
			break;
		case GameKeyEnum::ESC:
			if (Global::ConsoleBool) {
				Global::ConsoleBool = false;
				InterFace->ConsoleFocusHere = true;
			}else{
				InterFace->SetInterFaceBool();
			}
			break;
		case GameKeyEnum::Key_1:
			AttackType--;
			if (AttackType > SquarePhysics::DestroyModeEnumNumber) {
				AttackType = SquarePhysics::DestroyModeEnumNumber;
			}
			break;
		case GameKeyEnum::Key_2:
			AttackType++;
			if (AttackType > SquarePhysics::DestroyModeEnumNumber) {
				AttackType = 0;
			}
			break;
		default:
			break;
		}
	}

	void UnlimitednessMapMods::GameCommandBuffers(unsigned int Format_i)
	{
		mDungeon->GetCommandBuffer(wThreadCommandBufferS, Format_i);
		mDungeon->GetGIFCommandBuffer(wThreadCommandBufferS, Format_i);

		mParticleSystem->GetCommandBuffer(wThreadCommandBufferS, Format_i);
		//加到显示数组中
		mCrowd->GetCommandBufferS(wThreadCommandBufferS, Format_i);

		if (Global::MistSwitch) {
			mDungeon->GetMistCommandBuffer(wThreadCommandBufferS, Format_i);
		}

		mVisualEffect->GetCommandBuffer(wThreadCommandBufferS, Format_i);

		wThreadCommandBufferS->push_back(mGamePlayer->getCommandBuffer(Format_i));

		mAuxiliaryVision->GetCommandBuffer(wThreadCommandBufferS, Format_i);
	}

	void UnlimitednessMapMods::GameLoop(unsigned int mCurrentFrame)
	{
		mCrowd->NPCEvent(mCurrentFrame, TOOL::FPStime);

		Global::GamePlayerX = mGamePlayer->GetObjectCollision()->GetPosX();
		Global::GamePlayerY = mGamePlayer->GetObjectCollision()->GetPosY();

		int winwidth, winheight;
		glfwGetWindowSize(mWindow->getWindow(), &winwidth, &winheight);
		glfwGetCursorPos(mWindow->getWindow(), &CursorPosX, &CursorPosY);
		m_angle = std::atan2((winwidth / 2) - CursorPosX, (winheight / 2) - CursorPosY) + 1.57f;//获取角度

		glm::vec3 huoqdedian = get_ray_direction(CursorPosX, CursorPosY, winwidth, winheight, mCamera->getViewMatrix(), mCamera->getProjectMatrix());
		huoqdedian *= -mCamera->getCameraPos().z / huoqdedian.z;
		huoqdedian.x += mCamera->getCameraPos().x;
		huoqdedian.y += mCamera->getCameraPos().y;

		mVisualEffect->SetPos(((int(huoqdedian.x) / 16) + (huoqdedian.x < 0 ? -1 : 0)) * 16 + 8, ((int(huoqdedian.y) / 16) + (huoqdedian.y < 0 ? -1 : 0)) * 16 + 8, 0, mCurrentFrame);


		mGamePlayer->GetObjectCollision()->PlayerTargetAngle(m_angle);//设置玩家物理角度
		mGamePlayer->GetObjectCollision()->SufferForce(PlayerForce);//设置玩家受力
		PlayerForce = { 0, 0 };
		mAuxiliaryVision->Begin();
		mSquarePhysics->PhysicsSimulation(TOOL::FPStime);//物理事件



		m_angle = mGamePlayer->GetObjectCollision()->GetAngleFloat();

		mGamePlayer->UpData();//更新玩家伤痕
		mCamera->setCameraPos(mGamePlayer->GetObjectCollision()->GetPos());//设置玩家位置

		mParticlesSpecialEffect->SpecialEffectsEvent(mCurrentFrame, TOOL::FPStime);
		
		//更新Camera变换矩阵
		VPMatrices* mVPMatrices = (VPMatrices*)mCameraVPMatricesBuffer[mCurrentFrame]->getupdateBufferByMap();
		mVPMatrices->mViewMatrix = mCamera->getViewMatrix();//获取ViewMatrix数据
		mCameraVPMatricesBuffer[mCurrentFrame]->endupdateBufferByMap();


		mGamePlayer->setGamePlayerMatrix(TOOL::FPStime, mCurrentFrame);

		static double ArmsContinuityFire = 0;
		ArmsContinuityFire += TOOL::FPStime;
		static int zuojian;
		static SquarePhysics::ObjectSufferForce LSObjectDecorator{ nullptr, {0,0} };
		int Lzuojian = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_LEFT);
		if ((Lzuojian == GLFW_PRESS) && ((zuojian != Lzuojian) || ((ArmsContinuityFire > mArms->IntervalTime) && LSObjectDecorator.Object == nullptr)))
		{
			ArmsContinuityFire = 0;
			LSObjectDecorator = mSquarePhysics->GetGoods({ huoqdedian.x, huoqdedian.y });
			if (LSObjectDecorator.Object == nullptr)
			{
				glm::dvec2 Armsdain = SquarePhysics::vec2angle(glm::dvec2{ 9.0f, 0.0f }, m_angle);
				mArms->Shoot(mCamera->getCameraPos().x + Armsdain.x, mCamera->getCameraPos().y + Armsdain.y, m_angle, 500, AttackType);
			}
		}
		zuojian = Lzuojian;

		//是否有受力对象
		if (LSObjectDecorator.Object != nullptr) {
			glm::vec2 LSArmOfForce = SquarePhysics::vec2angle(LSObjectDecorator.ArmOfForce, LSObjectDecorator.Object->GetAngle());
			LSObjectDecorator.Object->ForceSolution(
				LSArmOfForce,
				glm::vec2{ huoqdedian.x - (LSObjectDecorator.Object->GetPosX() + LSArmOfForce.x), huoqdedian.y - (LSObjectDecorator.Object->GetPosY() + LSArmOfForce.y) },
				TOOL::FPStime
			);
			mAuxiliaryVision->Line(
				{ LSArmOfForce + LSObjectDecorator.Object->GetPos(), 0 },
				{ 1.0f, 0, 0, 1.0f },
				{ huoqdedian.x, huoqdedian.y, 0 },
				{ 0, 1.0f, 0, 1.0f }
			);
			mAuxiliaryVision->Spot(
				{ LSArmOfForce + LSObjectDecorator.Object->GetPos(), 0 },
				{ 0, 0, 1.0f, 1.0f }
			);
			mVisualEffect->SetPosAngle(LSObjectDecorator.Object->GetPosX(), LSObjectDecorator.Object->GetPosY(), 0, LSObjectDecorator.Object->GetAngleFloat(), mCurrentFrame);
			if (Lzuojian != GLFW_PRESS) {
				LSObjectDecorator.Object = nullptr;
			}
		}

		static glm::ivec2 beang{ 0 }, end{ 0 };
		static std::vector<JPSVec2> JPSPath;
		static std::vector<AStarVec2> AStarPath;
		//点击左键
		if (glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
			beang = { huoqdedian.x, huoqdedian.y };
			//int xpiany = (mDungeon->mMoveTerrain->OriginX - mDungeon->mMoveTerrain->GetGridSPosX()) * 16;
			//int ypiany = (mDungeon->mMoveTerrain->OriginY - mDungeon->mMoveTerrain->GetGridSPosY()) * 16;
			//std::cout << beang.x + xpiany << " - " << beang.y + xpiany << std::endl;
		}
		//点击右键
		static int fangzhifanfuvhufa;
		int Leftan = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_RIGHT);
		if (Leftan == GLFW_PRESS && fangzhifanfuvhufa != Leftan) {
			end = { huoqdedian.x, huoqdedian.y };
			//mGamePlayer->GetObjectCollision()->SetPos(mGamePlayer->GetObjectCollision()->GetPos() + glm::vec2{ 16 , -16});
			AStarPath.clear();
			JPSPath.clear();
			TOOL::mTimer->MomentTiming("AStar寻路耗时");
			AStarPathfinding->FindPath({ beang.x, beang.y }, { end.x, end.y }, &AStarPath, { mDungeon->PathfindingDecoratorDeviationX, mDungeon->PathfindingDecoratorDeviationY });
			TOOL::mTimer->MomentEnd();
			TOOL::mTimer->MomentTiming("JPS寻路耗时");
			JPSPathfinding->FindPath({ beang.x, beang.y }, { end.x, end.y }, &JPSPath, { mDungeon->PathfindingDecoratorDeviationX, mDungeon->PathfindingDecoratorDeviationY });
			TOOL::mTimer->MomentEnd();
			VulKan::StaticAuxiliaryData* PA = mAuxiliaryVision->GetContinuousStaticSpot()->Get(&AStarPath);
			PA->Size = AStarPath.size();
			PA->Pointer = &AStarPath;
			PA->Function = [](VulKan::AuxiliarySpot* P, void* D, unsigned int Size)->unsigned int {
				std::vector<AStarVec2>* DataP = (std::vector<AStarVec2>*)D;
				for (auto& i : *DataP)
				{
					P->Pos = { i.x, i.y, 0 };
					P->Color = { 0, 1.0f, 0, 1.0f };
					++P;
				}
				return DataP->size();
				};
			mAuxiliaryVision->OpenStaticSpotUpData();

			VulKan::StaticAuxiliaryData* P = mAuxiliaryVision->GetContinuousStaticLine()->Get(&JPSPath);
			P->Size = JPSPath.size();
			P->Pointer = &JPSPath;
			P->Function = [](VulKan::AuxiliarySpot* P, void* D, unsigned int Size)->unsigned int {
				std::vector<JPSVec2>* DataP = (std::vector<JPSVec2>*)D;
				for (auto& i : *DataP)
				{
					if ((i != (*DataP)[0]) && (i != DataP->back())) {
						P->Pos = { i.x, i.y, 0 };
						P->Color = { 0, 0, 1.0f, 1.0f };
						++P;
					}
					P->Pos = { i.x, i.y, 0 };
					P->Color = { 0, 0, 1.0f, 1.0f };
					++P;
				}
				return (DataP->size() * 2) - 2;
				};
			mAuxiliaryVision->OpenStaticLineUpData();
		}
		fangzhifanfuvhufa = Leftan;
		mAuxiliaryVision->Spot(
			{ beang, 0 },
			{ 0, 1.0f, 0, 1.0f }
		);
		mAuxiliaryVision->Spot(
			{ end, 0 },
			{ 1.0f, 0, 0, 1.0f }
		);

		mArms->BulletsEvent();

		mCrowd->TimeoutDetection();//检测玩家更新情况


		static double MistContinuityFire = 0;
		static float UpDataGIFTime = 0;
		UpDataGIFTime += TOOL::FPStime;
		if (UpDataGIFTime > 0.1f) {
			UpDataGIFTime = 0;
			mDungeon->UpDataGIF();
		}
		MovePlateInfo LMovePlateInfo = mDungeon->UpPos(mGamePlayer->GetObjectCollision()->GetPosX(), mGamePlayer->GetObjectCollision()->GetPosY());
		if (LMovePlateInfo.UpData) {
			mDungeon->UpdataMistData(LMovePlateInfo.X, LMovePlateInfo.Y);
			Global::MainCommandBufferUpdateRequest();
		}
		//战争迷雾
		MistContinuityFire += TOOL::FPStime;
		if (MistContinuityFire > 0.02f && Global::MistSwitch) {
			MistContinuityFire = 0;
			mDungeon->UpdataMist(int(mCamera->getCameraPos().x), int(mCamera->getCameraPos().y), m_angle + 0.7853981633975f - 1.57f);
		}

		mAuxiliaryVision->End();
	}

	void UnlimitednessMapMods::GameRecordCommandBuffers()
	{
		mAuxiliaryVision->initCommandBuffer();
		mGamePlayer->InitCommandBuffer();
		mCrowd->ReconfigurationCommandBuffer();
		mVisualEffect->initCommandBuffer();
		mDungeon->initCommandBuffer();
	}

	//游戏停止界面循环
	void UnlimitednessMapMods::GameStopInterfaceLoop(unsigned int mCurrentFrame) {

	}

	//游戏 TCP事件
	void UnlimitednessMapMods::GameTCPLoop() {

	}
}
