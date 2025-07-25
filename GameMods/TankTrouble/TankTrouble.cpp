#include "TankTrouble.h"
#include "../../NetworkTCP/Server.h"
#include "../../NetworkTCP/Client.h"
#include "../../Opcode/OpcodeFunction.h"
#include "../../Physics/DestroyMode.h"

namespace GAME {

#define VirtualBool 1;

	class TankTroubleJPS : public JPS
	{
	public:
		TankTroubleJPS(int Range, int Steps, FixedMaze* c) : JPS(Range, Steps), CLASS(c){};
		~TankTroubleJPS() {};

		virtual inline bool isValid(int x, int y) {
			if (!Legitimacy(x, y)) {
				return false;
			}
			return CLASS->GetPixelWallNumber(x + StartingPoint.x, y + StartingPoint.y);
		}

	private:
		FixedMaze* CLASS;
	};
	TankTroubleJPS* mTankTroubleJPS = nullptr;

	bool TankTroubleGetWallD_2(int x, int y, void* P) {
		FixedMaze* LLabyrinth = (FixedMaze*)P;
		return LLabyrinth->GetPixelWallNumber(x, y);
	}

	TankTrouble::TankTrouble(Configuration wConfiguration) : Configuration{ wConfiguration }
	{
		mAuxiliaryVision = new VulKan::AuxiliaryVision(mDevice, mPipelineS, 1000);
		mAuxiliaryVision->initUniformManager(
			mSwapChain->getImageCount(),
			mCameraVPMatricesBuffer
		);
		mAuxiliaryVision->RecordingCommandBuffer(mRenderPass, mSwapChain);

		mDamagePrompt = new DamagePrompt(mDevice, mPipelineS->GetPipeline(VulKan::PipelineMods::DamagePrompt)->DescriptorSetLayout, mCameraVPMatricesBuffer,
			mTextureLibrary->GetTextureUV("DamagePrompt").mTexture, mSwapChain->getImageCount(), 10);
		mDamagePrompt->RecordingCommandBuffer(mRenderPass, mSwapChain, mPipelineS->GetPipeline(VulKan::PipelineMods::DamagePrompt));
		
		mUVDynamicDiagram = new UVDynamicDiagram(mDevice, mPipelineS->GetPipeline(VulKan::PipelineMods::UVDynamicDiagram), mSwapChain, mRenderPass, mCameraVPMatricesBuffer, mSquarePhysics);
		mUVDynamicDiagram->InitCommandBuffer();


		//生成迷宫
		mLabyrinth = new FixedMaze(mSquarePhysics);
		mLabyrinth->InitLabyrinth(mDevice, 21, 21);
		mLabyrinth->initUniformManager(
			mDevice,
			mSwapChain->getImageCount(),
			mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods)->DescriptorSetLayout,
			&mCameraVPMatricesBuffer,
			mSampler
		);
		mLabyrinth->RecordingCommandBuffer(mRenderPass, mSwapChain, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods));

		//测试寻路

		// 这种方法在查询是否是障碍物最快（内联函数，不是回调函数）
		mTankTroubleJPS = new TankTroubleJPS(300, 10000, mLabyrinth);

		JPSPathfinding = new JPS(300, 10000);
		AStarPathfinding = new AStar(300, 10000);
		JPSPathfinding->SetObstaclesCallback(TankTroubleGetWallD_2, mLabyrinth);
		AStarPathfinding->SetObstaclesCallback(TankTroubleGetWallD_2, mLabyrinth);

		

		glm::ivec2 Lpos = mLabyrinth->GetLegitimateGeneratePos();


		//创建多人玩家
		mCrowd = new Crowd(100, mDevice, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods), mSwapChain, mRenderPass, mSampler, mCameraVPMatricesBuffer, mLabyrinth);
		mCrowd->SteSquarePhysics(mSquarePhysics);
		mCrowd->SetArms(mArms);


		//创建玩家
		mGamePlayer = new GamePlayer(mDevice, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods), mSwapChain, mRenderPass, mSquarePhysics, Lpos.x, Lpos.y);
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
		mGamePlayer->SetDamagePrompt(mDamagePrompt);

		VulKan::AuxiliaryForceData* ALine = mAuxiliaryVision->GetContinuousForce()->New(mGamePlayer->GetObjectCollision()->GetForcePointer());
		ALine->pos = mGamePlayer->GetObjectCollision()->GetPosPointer();
		ALine->Force = mGamePlayer->GetObjectCollision()->GetForcePointer();
		ALine->Color = { 0, 0, 1.0f, 1.0f };
		ALine = mAuxiliaryVision->GetContinuousForce()->New(mGamePlayer->GetObjectCollision()->GetSpeedPointer());
		ALine->pos = mGamePlayer->GetObjectCollision()->GetPosPointer();
		ALine->Force = mGamePlayer->GetObjectCollision()->GetSpeedPointer();
		ALine->Color = { 0, 1.0f, 0, 1.0f };

		if (Global::MultiplePeopleMode)
		{
			if (Global::ServerOrClient) {
				server::GetServer()->SetArms(mArms);
				server::GetServer()->SetCrowd(mCrowd);
				server::GetServer()->SetGamePlayer(mGamePlayer);
				//server::GetServer()->SetLabyrinth(mLabyrinth);
				server::GetServer()->SetInterFace(InterFace);
				if (mGamePlayer->GetRoleSynchronizationData() == nullptr) {
					RoleSynchronizationData* LRole = server::GetServer()->GetServerData()->New(0);
					LRole->Key = 0;
					LRole->mBufferEventSingleData = new BufferEventSingleData(100);
					mGamePlayer->SetRoleSynchronizationData(LRole);
					server::GetServer()->GetServerData()->SetPointerData(0, mGamePlayer);
				}
			}
			else {
				client::GetClient()->SetArms(mArms);
				client::GetClient()->SetCrowd(mCrowd);
				client::GetClient()->SetGamePlayer(mGamePlayer);
				//client::GetClient()->SetLabyrinth(mLabyrinth);
				client::GetClient()->SetInterFace(InterFace);
			}
		}

		//给操作码对象赋值
		Opcode::OpArms = mArms;
		//Opcode::OpLabyrinth = mLabyrinth;
		Opcode::OpCrowd = mCrowd;
		Opcode::OpGamePlayer = mGamePlayer;
		Opcode::OpImGuiInterFace = InterFace;

		mCrowd->AddNPC(-208, 60);
	}

	TankTrouble::~TankTrouble()
	{
		delete JPSPathfinding;
		delete AStarPathfinding;
		delete mAuxiliaryVision;
		delete mLabyrinth;
		delete mCrowd;
		delete mGamePlayer;
		delete mDamagePrompt;
		delete mUVDynamicDiagram;

		if (Global::MultiplePeopleMode)
		{
			if (Global::ServerOrClient) {
				delete server::GetServer();
			}
			else {
				delete client::GetClient();
			}
			Global::MultiplePeopleMode = false;
		}
	}

	void TankTrouble::MouseMove(double xpos, double ypos)
	{
		//mCamera->onMouseMove(xpos, ypos);
	}

	void TankTrouble::MouseRoller(int z)
	{
		m_position.z += z * 10;
		mCamera->update();
	}

	void TankTrouble::KeyDown(GameKeyEnum moveDirection)
	{
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
			}else {
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

	void TankTrouble::GameCommandBuffers(unsigned int Format_i)
	{
		mLabyrinth->GetCommandBuffer(wThreadCommandBufferS, Format_i);

		mParticleSystem->GetCommandBuffer(wThreadCommandBufferS, Format_i);
		//加到显示数组中
		mCrowd->GetCommandBufferS(wThreadCommandBufferS, Format_i);

		wThreadCommandBufferS->push_back(mGamePlayer->getCommandBuffer(Format_i));

		mAuxiliaryVision->GetCommandBuffer(wThreadCommandBufferS, Format_i);

		mDamagePrompt->GetCommandBuffer(wThreadCommandBufferS, Format_i);

		mUVDynamicDiagram->GetCommandBuffer(wThreadCommandBufferS, Format_i);
	}

	void TankTrouble::GameLoop(unsigned int mCurrentFrame)
	{
		mAuxiliaryVision->Begin();

		Global::GamePlayerX = mGamePlayer->GetObjectCollision()->GetPosX();
		Global::GamePlayerY = mGamePlayer->GetObjectCollision()->GetPosY();

		mDamagePrompt->UpDataDamagePrompt(Global::GamePlayerX, Global::GamePlayerY, TOOL::FPStime);

		for (size_t i = 0; i < mUVDynamicDiagram->mIndexAnimationGrid->GetOutlinePointSize(); i++)
		{
			mAuxiliaryVision->AddSpot(
				{
					SquarePhysics::vec2angle(
						mUVDynamicDiagram->mIndexAnimationGrid->GetOutlinePointSet(i),
						mUVDynamicDiagram->mIndexAnimationGrid->GetAngle()
					) + glm::dvec2(mUVDynamicDiagram->mIndexAnimationGrid->GetPos()),
					0
				},
				0.2,
				glm::vec4{1,0,0,1}
			);
		}
			
		mUVDynamicDiagram->AnimationEvent(TOOL::FPStime);
		if (!Global::ServerOrClient)
		{
			mCrowd->NPCTimeoutDetection();
		}
		else
		{
			mCrowd->NPCEvent(mCurrentFrame, TOOL::FPStime);
		}

		int winwidth, winheight;
		glfwGetWindowSize(mWindow->getWindow(), &winwidth, &winheight);
		glfwGetCursorPos(mWindow->getWindow(), &CursorPosX, &CursorPosY);
		m_angle = std::atan2((winwidth / 2) - CursorPosX, (winheight / 2) - CursorPosY) + 1.57f;//获取角度

		glm::vec3 huoqdedian = get_ray_direction(CursorPosX, CursorPosY, winwidth, winheight, mCamera->getViewMatrix(), mCamera->getProjectMatrix());
		huoqdedian *= -mCamera->getCameraPos().z / huoqdedian.z;
		huoqdedian.x += mCamera->getCameraPos().x;
		huoqdedian.y += mCamera->getCameraPos().y;



		mGamePlayer->GetObjectCollision()->PlayerTargetAngle(m_angle);//设置玩家物理角度
		mGamePlayer->GetObjectCollision()->SufferForce(PlayerForce);//设置玩家受力
		PlayerForce = { 0, 0 };
		TOOL::mTimer->StartTiming(u8"物理模拟 ", true);
		mSquarePhysics->PhysicsSimulation(TOOL::FPStime);//物理事件
		TOOL::mTimer->StartEnd();

		

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
				if (Global::MultiplePeopleMode) {//是否为多人模式
					if (Global::ServerOrClient) {//服务器还是客户端
						RoleSynchronizationData* LServerPos = server::GetServer()->GetServerData()->GetKeyData(0);
						BufferEventSingleData* LBufferEventSingleData;
						for (size_t i = 0; i < server::GetServer()->GetServerData()->GetKeyNumber(); ++i)
						{
							LBufferEventSingleData = LServerPos[i].mBufferEventSingleData;
							LBufferEventSingleData->mSubmitBullet->add({ float(mCamera->getCameraPos().x + Armsdain.x), float(mCamera->getCameraPos().y + Armsdain.y), m_angle, AttackType });
						}
					}
					else {
						client::GetClient()->GetGamePlayer()->GetRoleSynchronizationData()->mBufferEventSingleData->mSubmitBullet->add({ float(mCamera->getCameraPos().x + Armsdain.x), float(mCamera->getCameraPos().y + Armsdain.y), m_angle, AttackType });
					}
				}
			}
		}
		zuojian = Lzuojian;

		//是否有受力对象
		if (LSObjectDecorator.Object != nullptr) {
			glm::dvec2 LSArmOfForce = SquarePhysics::vec2angle(LSObjectDecorator.ArmOfForce, LSObjectDecorator.Object->GetAngle());
			LSObjectDecorator.Object->ForceSolution(
				LSArmOfForce,
				glm::dvec2{ huoqdedian.x - (LSObjectDecorator.Object->GetPosX() + LSArmOfForce.x), huoqdedian.y - (LSObjectDecorator.Object->GetPosY() + LSArmOfForce.y) },
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
				0.25f,
				{ 0, 0, 1.0f, 1.0f }
			);
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
		}
		//点击右键
		static int fangzhifanfuvhufa;
		int Leftan = glfwGetMouseButton(mWindow->getWindow(), GLFW_MOUSE_BUTTON_RIGHT);
		if (Leftan == GLFW_PRESS && fangzhifanfuvhufa != Leftan) {
			end = { huoqdedian.x, huoqdedian.y };
			AStarPath.clear();
			JPSPath.clear();
			
			TOOL::mTimer->MomentTiming("AStar寻路耗时");
			AStarPathfinding->FindPath({ beang.x, beang.y }, { end.x, end.y }, &AStarPath);
			TOOL::mTimer->MomentEnd();
			TOOL::mTimer->MomentTiming("JPS寻路耗时");
			mTankTroubleJPS->FindPath({ beang.x, beang.y }, { end.x, end.y }, &JPSPath);
			TOOL::mTimer->MomentEnd();
			
			VulKan::StaticAuxiliarySpotData* PA = mAuxiliaryVision->GetContinuousStaticSpot()->Get(&AStarPath);
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

			VulKan::StaticAuxiliaryLineData* P = mAuxiliaryVision->GetContinuousStaticLine()->Get(&JPSPath);
			P->Size = JPSPath.size();
			P->Pointer = &JPSPath;
			P->Function = [](VulKan::AuxiliaryLineSpot* P, void* D, unsigned int Size)->unsigned int {
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
			0.25f,
			{ 0, 1.0f, 0, 1.0f }
		);
		mAuxiliaryVision->Spot(
			{ end, 0 },
			0.25f,
			{ 1.0f, 0, 0, 1.0f }
		);
		
		mArms->BulletsEvent();
		
		mCrowd->TimeoutDetection();//检测玩家更新情况
		

		static double MistContinuityFire = 0;
		mLabyrinth->UpDateMaps();
		mAuxiliaryVision->End();
	}

	void TankTrouble::GameRecordCommandBuffers()
	{
		mAuxiliaryVision->initCommandBuffer();
		mGamePlayer->InitCommandBuffer();
		mCrowd->ReconfigurationCommandBuffer();
		mLabyrinth->ThreadUpdateCommandBuffer();
		mDamagePrompt->initCommandBuffer();
		mUVDynamicDiagram->InitCommandBuffer();
	}

	//游戏停止界面循环
	void TankTrouble::GameStopInterfaceLoop(unsigned int mCurrentFrame) {
		if (InterFace->GetInterfaceIndexes() == InterFaceEnum_::ViceInterface_Enum && Global::MultiplePeopleMode && !Global::ServerOrClient) {
			mCrowd->UpTime();
		}
	}

	//游戏 TCP事件
	void TankTrouble::GameTCPLoop() {
		if (Global::MultiplePeopleMode)
		{
			if (Global::ServerOrClient) {
				event_base_loop(server::GetServer()->GetEvent_Base(), EVLOOP_NONBLOCK);
			}
			else {
				event_base_loop(client::GetClient()->GetEvent_Base(), EVLOOP_ONCE);
			}
		}
	}

}
