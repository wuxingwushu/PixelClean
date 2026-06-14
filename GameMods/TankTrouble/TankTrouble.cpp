#include "TankTrouble.h"
#include "../../Opcode/OpcodeFunction.h"
// DestroyMode now in game layer (old: #include "../../Physics/DestroyMode.h")
#include "../../DebugLog.h"

namespace GAME
{

#define VirtualBool 1;

	class TankTroubleJPS : public JPS
	{
	public:
		TankTroubleJPS(int Range, int Steps, FixedMaze *c) : JPS(Range, Steps), CLASS(c) {};
		~TankTroubleJPS() {};

		virtual inline bool isValid(int x, int y)
		{
			if (!Legitimacy(x, y))
			{
				return false;
			}
			return CLASS->GetPixelWallNumber(x + StartingPoint.x, y + StartingPoint.y);
		}

	private:
		FixedMaze *CLASS;
	};
	TankTroubleJPS *mTankTroubleJPS = nullptr;

	bool TankTroubleGetWallD_2(int x, int y, void *P)
	{
		FixedMaze *LLabyrinth = (FixedMaze *)P;
		return LLabyrinth->GetPixelWallNumber(x, y);
	}

	TankTrouble::TankTrouble(Configuration wConfiguration) : Configuration{wConfiguration}
	{
		LOGD("TankTrouble::TankTrouble constructor");
		mAuxiliaryVision = new VulKan::AuxiliaryVision(mDevice, mPipelineS, 1000);
		mAuxiliaryVision->initUniformManager(
			mSwapChain->getImageCount(),
			mCameraVPMatricesBuffer);
		mAuxiliaryVision->RecordingCommandBuffer(mRenderPass, mSwapChain);

		mDamagePrompt = new DamagePrompt(mDevice, mPipelineS->GetPipeline(VulKan::PipelineMods::DamagePrompt)->DescriptorSetLayout, mCameraVPMatricesBuffer,
										 mTextureLibrary->GetTextureUV("DamagePrompt").mTexture, mSwapChain->getImageCount(), 10);
		mDamagePrompt->RecordingCommandBuffer(mRenderPass, mSwapChain, mPipelineS->GetPipeline(VulKan::PipelineMods::DamagePrompt));

		mUVDynamicDiagram = new UVDynamicDiagram(mDevice, mPipelineS->GetPipeline(VulKan::PipelineMods::UVDynamicDiagram), mSwapChain, mRenderPass, mCameraVPMatricesBuffer, mSquarePhysics);
		mUVDynamicDiagram->InitCommandBuffer();

		// 生成迷宫
		mLabyrinth = new FixedMaze(mSquarePhysics);
		mLabyrinth->InitLabyrinth(mDevice, 21, 21);
		mLabyrinth->initUniformManager(
			mDevice,
			mSwapChain->getImageCount(),
			mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods)->DescriptorSetLayout,
			&mCameraVPMatricesBuffer,
			mSampler);
		mLabyrinth->RecordingCommandBuffer(mRenderPass, mSwapChain, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods));

		// 测试寻路

		// 这种方法在查询是否是障碍物最快（内联函数，不是回调函数）
		mTankTroubleJPS = new TankTroubleJPS(300, 10000, mLabyrinth);

		JPSPathfinding = new JPS(300, 10000);
		AStarPathfinding = new AStar(300, 10000);
		JPSPathfinding->SetObstaclesCallback(TankTroubleGetWallD_2, mLabyrinth);
		AStarPathfinding->SetObstaclesCallback(TankTroubleGetWallD_2, mLabyrinth);

		glm::ivec2 Lpos = mLabyrinth->GetLegitimateGeneratePos();

		// 创建多人玩家
		mCrowd = new Crowd(100, mDevice, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods), mSwapChain, mRenderPass, mSampler, mCameraVPMatricesBuffer, mLabyrinth);
		mCrowd->SteSquarePhysics(mSquarePhysics);
		mCrowd->SetArms(mArms);

		// 创建玩家
		mGamePlayer = new GamePlayer(mDevice, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods), mSwapChain, mRenderPass, mSquarePhysics, Lpos.x, Lpos.y);
		mGamePlayer->initUniformManager(
			mDevice,
			mCommandPool,
			mSwapChain->getImageCount(),
			10,
			mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods)->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler);
		mGamePlayer->InitCommandBuffer();
		mGamePlayer->SetDamagePrompt(mDamagePrompt);

		VulKan::AuxiliaryForceData *ALine = mAuxiliaryVision->GetContinuousForce()->New(&mGamePlayer->GetObjectCollision()->force);
		ALine->pos = &mGamePlayer->GetObjectCollision()->pos;
		ALine->Force = &mGamePlayer->GetObjectCollision()->force;
		ALine->Color = {0, 0, 1.0f, 1.0f};
		ALine = mAuxiliaryVision->GetContinuousForce()->New(&mGamePlayer->GetObjectCollision()->speed);
		ALine->pos = &mGamePlayer->GetObjectCollision()->pos;
		ALine->Force = &mGamePlayer->GetObjectCollision()->speed;
		ALine->Color = {0, 1.0f, 0, 1.0f};

		// 给操作码对象赋值
		Opcode::OpArms = mArms;
		// Opcode::OpLabyrinth = mLabyrinth;
		Opcode::OpCrowd = mCrowd;
		Opcode::OpGamePlayer = mGamePlayer;
		Opcode::OpImGuiInterFace = InterFace;

		mCrowd->AddNPC(-208, 60);
	}

	TankTrouble::~TankTrouble()
	{
		LOGD("TankTrouble::~TankTrouble destructor");
		delete JPSPathfinding;
		delete AStarPathfinding;
		delete mAuxiliaryVision;
		delete mCrowd;          // 必须在 mLabyrinth 之前：NPC 的 JPS 线程使用 Labyrinth 的网格数据
		delete mGamePlayer;     // 必须在 mLabyrinth 之前：GamePlayer 析构时 mSquarePhysics 仍需要有效
		delete mLabyrinth;
		delete mDamagePrompt;
		delete mUVDynamicDiagram;

		Global::MultiplePeopleMode = false;
	}

	void TankTrouble::MouseMove(double xpos, double ypos)
	{
		CursorPosX = xpos;
		CursorPosY = ypos;
#if defined(__ANDROID__)
		mWinWidth = mWindow->getWidth();
		mWinHeight = mWindow->getHeight();
#else
		glfwGetWindowSize(mWindow->getWindow(), &mWinWidth, &mWinHeight);
#endif
	}

	void TankTrouble::MouseButton(MouseBtn button, InputState State)
	{
		switch (button) {
		case MouseBtn::Left:
			if (State == InputState::Down || State == InputState::Hold)
				mLeftMouseDown = true;
			else
				mLeftMouseDown = false;
			break;
		case MouseBtn::Right:
			if (State == InputState::Down || State == InputState::Hold)
				mRightMouseDown = true;
			else
				mRightMouseDown = false;
			break;
		default:
			break;
		}
	}

	void TankTrouble::MouseRoller(int z)
	{
		// 在 imgui 窗口上所以不响应
		if (Global::ClickWindow)
			return;

		if (m_position.z <= 10)
		{
			m_position.z += (m_position.z / 2) * z;
		}
		else
		{
			m_position.z += z * 5;
		}
		if (m_position.z <= 0.1)
		{
			m_position.z = 0.1;
		}
		mCamera->update();
	}

	void TankTrouble::KeyDown(GameKeyEnum moveDirection)
	{
		const float lidaxiao = 100 * TOOL::FPStime;
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
			if (Global::ConsoleBool)
			{
				Global::ConsoleBool = false;
				InterFace->ConsoleFocusHere = true;
			}
			else
			{
				InterFace->SetInterFaceBool();
			}
			break;
		case GameKeyEnum::Key_1:
			AttackType--;
			{
				static constexpr int DESTROY_MODE_COUNT = 4;
				if (AttackType >= DESTROY_MODE_COUNT) { AttackType = DESTROY_MODE_COUNT - 1; }
			}
			break;
		case GameKeyEnum::Key_2:
			AttackType++;
			{
				static constexpr int DESTROY_MODE_COUNT = 4;
				if (AttackType >= DESTROY_MODE_COUNT) { AttackType = DESTROY_MODE_COUNT - 1; }
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
		// 加到显示数组中
		mCrowd->GetCommandBufferS(wThreadCommandBufferS, Format_i);

		wThreadCommandBufferS->push_back(mGamePlayer->getCommandBuffer(Format_i));

		mAuxiliaryVision->GetCommandBuffer(wThreadCommandBufferS, Format_i);

		mDamagePrompt->GetCommandBuffer(wThreadCommandBufferS, Format_i);

		mUVDynamicDiagram->GetCommandBuffer(wThreadCommandBufferS, Format_i);
	}

	void TankTrouble::GameLoop(unsigned int mCurrentFrame)
	{
		mAuxiliaryVision->Begin();

		Global::GamePlayerX = mGamePlayer->GetObjectCollision()->pos.x;
		Global::GamePlayerY = mGamePlayer->GetObjectCollision()->pos.y;

		mDamagePrompt->UpDataDamagePrompt(Global::GamePlayerX, Global::GamePlayerY, TOOL::FPStime);

		// TODO: mIndexAnimationGrid removed from new engine; animation grid outline points need redesign
		/*
		for (size_t i = 0; i < mUVDynamicDiagram->mIndexAnimationGrid->GetOutlinePointSize(); i++)
		{
			mAuxiliaryVision->AddSpot(
				{PhysicsBlock::vec2angle(
					 mUVDynamicDiagram->mIndexAnimationGrid->GetOutlinePointSet(i),
					 mUVDynamicDiagram->mIndexAnimationGrid->GetAngle()) +
					 glm::dvec2(mUVDynamicDiagram->mIndexAnimationGrid->GetPos()),
				 0},
				0.2,
				glm::vec4{1, 0, 0, 1});
		}
		*/

		// TODO: AnimationEvent temporarily disabled - IndexAnimationGrid is old engine concept
		// mUVDynamicDiagram->AnimationEvent(TOOL::FPStime);
		if (!Global::ServerOrClient)
		{
			mCrowd->NPCTimeoutDetection();
		}
		else
		{
			mCrowd->NPCEvent(mCurrentFrame, TOOL::FPStime);
		}

		int winwidth = mWinWidth, winheight = mWinHeight;
		if (winwidth == 0 || winheight == 0) {
#if defined(__ANDROID__)
			winwidth = mWindow->getWidth();
			winheight = mWindow->getHeight();
#else
			glfwGetWindowSize(mWindow->getWindow(), &winwidth, &winheight);
#endif
		}
		m_angle = std::atan2((winwidth / 2) - CursorPosX, (winheight / 2) - CursorPosY) + 1.57f;

		glm::vec3 huoqdedian = get_ray_direction(CursorPosX, CursorPosY, winwidth, winheight, mCamera->getViewMatrix(), mCamera->getProjectMatrix());
		huoqdedian *= -mCamera->getCameraPos().z / huoqdedian.z;
		huoqdedian.x += mCamera->getCameraPos().x;
		huoqdedian.y += mCamera->getCameraPos().y;

		mGamePlayer->GetObjectCollision()->angle = m_angle; // 设置玩家物理角度
		mGamePlayer->GetObjectCollision()->PFSpeed() += PlayerForce;   // 设置玩家受力
		PlayerForce = {0, 0};
		TOOL::mTimer->StartTiming(u8"物理模拟 ", true);
		mSquarePhysics->PhysicsEmulator(TOOL::FPStime); // 物理事件
		TOOL::mTimer->StartEnd();

		m_angle = mGamePlayer->GetObjectCollision()->angle;

		mGamePlayer->UpData();												// 更新玩家伤痕
		mCamera->setCameraPos(mGamePlayer->GetObjectCollision()->pos); // 设置玩家位置

		mParticlesSpecialEffect->SpecialEffectsEvent(mCurrentFrame, TOOL::FPStime);

		// 更新Camera变换矩阵（使用持久映射避免每帧 map/unmap 开销）
		VPMatrices *mVPMatrices = (VPMatrices *)mCameraVPMatricesBuffer[mCurrentFrame]->getPersistentMappedPtr();
		mVPMatrices->mViewMatrix = mCamera->getViewMatrix(); // 获取ViewMatrix数据

		mGamePlayer->setGamePlayerMatrix(TOOL::FPStime, mCurrentFrame);

		static double ArmsContinuityFire = 0;
		ArmsContinuityFire += TOOL::FPStime;
		static int zuojian;
		static PhysicsBlock::PhysicsFormwork* LSObjectDecorator = nullptr;
		if (!Global::ClickWindow)
		{
			if ((mLeftMouseDown) && ((zuojian != (mLeftMouseDown ? 1 : 0)) || ((ArmsContinuityFire > mArms->IntervalTime) && LSObjectDecorator == nullptr)))
			{
				ArmsContinuityFire = 0;
				LSObjectDecorator = mSquarePhysics->Get(Vec2_{static_cast<FLOAT_>(huoqdedian.x), static_cast<FLOAT_>(huoqdedian.y)});
				if (LSObjectDecorator == nullptr)
				{
					glm::dvec2 Armsdain = PhysicsBlock::vec2angle(glm::dvec2{9.0f, 0.0f}, m_angle);
					mArms->Shoot(mCamera->getCameraPos().x + Armsdain.x, mCamera->getCameraPos().y + Armsdain.y, m_angle, 500, AttackType);
				}
			}
			zuojian = mLeftMouseDown ? 1 : 0;
		}

		// 是否有受力对象
		if (LSObjectDecorator != nullptr)
		{
			auto* angleObj = static_cast<PhysicsBlock::PhysicsAngle*>(LSObjectDecorator);
			glm::dvec2 LSArmOfForce = glm::dvec2{huoqdedian.x - angleObj->pos.x, huoqdedian.y - angleObj->pos.y};
			angleObj->AddForce(Vec2_{huoqdedian.x, huoqdedian.y},
				Vec2_{static_cast<FLOAT_>(LSArmOfForce.x * TOOL::FPStime), static_cast<FLOAT_>(LSArmOfForce.y * TOOL::FPStime)});
			mAuxiliaryVision->Line(
				glm::dvec3{LSArmOfForce + glm::dvec2(angleObj->pos), 0},
				{1.0f, 0, 0, 1.0f},
				glm::dvec3{huoqdedian.x, huoqdedian.y, 0},
				{0, 1.0f, 0, 1.0f});
			mAuxiliaryVision->Spot(
				glm::dvec3{LSArmOfForce + glm::dvec2(angleObj->pos), 0},
				0.25f,
				{0, 0, 1.0f, 1.0f});
			if (!mLeftMouseDown)
			{
				LSObjectDecorator = nullptr;
			}
		}

		static glm::ivec2 beang{0}, end{0};
		static std::vector<JPSVec2> JPSPath;
		static std::vector<AStarVec2> AStarPath;
		if (!Global::ClickWindow)
		{
			if (mLeftMouseDown)
			{
				beang = {huoqdedian.x, huoqdedian.y};
			}
			static int fangzhifanfuvhufa;
			if (mRightMouseDown && fangzhifanfuvhufa != (mRightMouseDown ? 1 : 0))
			{
				end = {huoqdedian.x, huoqdedian.y};
				AStarPath.clear();
				JPSPath.clear();

				TOOL::mTimer->MomentTiming("AStar寻路耗时");
				AStarPathfinding->FindPath({beang.x, beang.y}, {end.x, end.y}, &AStarPath);
				TOOL::mTimer->MomentEnd();
				TOOL::mTimer->MomentTiming("JPS寻路耗时");
				mTankTroubleJPS->FindPath({beang.x, beang.y}, {end.x, end.y}, &JPSPath);
				TOOL::mTimer->MomentEnd();

				VulKan::StaticAuxiliarySpotData *PA = mAuxiliaryVision->GetContinuousStaticSpot()->Get(&AStarPath);
				PA->Size = AStarPath.size();
				PA->Pointer = &AStarPath;
				PA->Function = [](VulKan::AuxiliarySpot *P, void *D, unsigned int Size) -> unsigned int
				{
					std::vector<AStarVec2> *DataP = (std::vector<AStarVec2> *)D;
					for (auto &i : *DataP)
					{
						P->Pos = {i.x, i.y, 0};
						P->Color = {0, 1.0f, 0, 1.0f};
						++P;
					}
					return DataP->size();
				};
				mAuxiliaryVision->OpenStaticSpotUpData();

				VulKan::StaticAuxiliaryLineData *P = mAuxiliaryVision->GetContinuousStaticLine()->Get(&JPSPath);
				P->Size = JPSPath.size();
				P->Pointer = &JPSPath;
				P->Function = [](VulKan::AuxiliaryLineSpot *P, void *D, unsigned int Size) -> unsigned int
				{
					std::vector<JPSVec2> *DataP = (std::vector<JPSVec2> *)D;
					for (auto &i : *DataP)
					{
						if ((i != (*DataP)[0]) && (i != DataP->back()))
						{
							P->Pos = {i.x, i.y, 0};
							P->Color = {0, 0, 1.0f, 1.0f};
							++P;
						}
						P->Pos = {i.x, i.y, 0};
						P->Color = {0, 0, 1.0f, 1.0f};
						++P;
					}
					return (DataP->size() * 2) - 2;
				};
				mAuxiliaryVision->OpenStaticLineUpData();
			}
			fangzhifanfuvhufa = mRightMouseDown ? 1 : 0;
		}
		mAuxiliaryVision->Spot(
			{beang, 0},
			0.25f,
			{0, 1.0f, 0, 1.0f});
		mAuxiliaryVision->Spot(
			{end, 0},
			0.25f,
			{1.0f, 0, 0, 1.0f});

		mArms->BulletsEvent();

		mCrowd->TimeoutDetection(); // 检测玩家更新情况

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

	// 游戏停止界面循环
	void TankTrouble::GameStopInterfaceLoop(unsigned int mCurrentFrame)
	{
		if (InterFace->GetInterfaceIndexes() == InterFaceEnum_::ViceInterface_Enum && Global::MultiplePeopleMode && !Global::ServerOrClient)
		{
			mCrowd->UpTime();
		}
	}

	// 游戏 TCP事件
	void TankTrouble::GameTCPLoop()
	{
	}

}
