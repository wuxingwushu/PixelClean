#include "UnlimitednessMapMods.h"
#include "../../Opcode/OpcodeFunction.h"
// DestroyMode now in game layer (old: #include "../../Physics/DestroyMode.h")
#include "../../DebugLog.h"

namespace GAME
{

	bool DungeonGetWallD(int x, int y, void *P)
	{
		Dungeon *LLabyrinth = (Dungeon *)P;
		return LLabyrinth->GetPixelWallNumber(x, y);
	}

	UnlimitednessMapMods::UnlimitednessMapMods(Configuration wConfiguration) : Configuration{wConfiguration}
	{
		LOGD("UnlimitednessMapMods::UnlimitednessMapMods constructor");

		// 创建模式私有物理世界
		mSquarePhysics = new PhysicsBlock::PhysicsWorld(Vec2_{0, 0}, false);
		// 创建模式私有武器系统
		mArms = new Arms(mParticleSystem, mParticleCount);
		mArms->SetSpecialEffect(mParticlesSpecialEffect);
		mArms->SetSquarePhysics(mSquarePhysics);

		mAuxiliaryVision = new VulKan::AuxiliaryVision(mDevice, mPipelineS, 150000);
		mAuxiliaryVision->initUniformManager(
			mSwapChain->getImageCount(),
			mCameraVPMatricesBuffer);
		mAuxiliaryVision->RecordingCommandBuffer(mRenderPass, mSwapChain);

		mDamagePrompt = new DamagePrompt(mDevice, mPipelineS->GetPipeline(VulKan::PipelineMods::DamagePrompt)->DescriptorSetLayout, mCameraVPMatricesBuffer,
										 mTextureLibrary->GetTextureUV("DamagePrompt").mTexture, mSwapChain->getImageCount(), 10);
		mDamagePrompt->RecordingCommandBuffer(mRenderPass, mSwapChain, mPipelineS->GetPipeline(VulKan::PipelineMods::DamagePrompt));

		// 测试寻路
		JPSPathfinding = new JPS(300, 10000);
		AStarPathfinding = new AStar(300, 10000);

		float mGamePlayerPosX = (50 * 16) / 2.0f;
		float mGamePlayerPosY = (30 * 16) / 2.0f;
		mDungeon = new Dungeon(mDevice, 50, 30, mSquarePhysics, mGamePlayerPosX, mGamePlayerPosY);

		// 绑定物理辅助显示（动态地图轮廓此时已注册进物理世界，板块随玩家移动会自动重绘）
		mPhysicsDebug.setup(mAuxiliaryVision, mSquarePhysics);
		mDungeon->initUniformManager(
			mSwapChain->getImageCount(),
			mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods)->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler,
			mGIFTextureLibrary);
		// 生成所有板块的初始碰撞数据（必须在 initUniformManager 之后，因为 GenerateBlock 的回调需要 WarfareMist/WallBool）
		mDungeon->GenerateInitialCollision(mGamePlayerPosX, mGamePlayerPosY);
		mDungeon->RecordingCommandBuffer(mRenderPass, mSwapChain, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods), mPipelineS->GetPipeline(VulKan::PipelineMods::GifMods));

		JPSPathfinding->SetObstaclesCallback(DungeonGetWallD, mDungeon);
		AStarPathfinding->SetObstaclesCallback(DungeonGetWallD, mDungeon);

		mVisualEffect = new VulKan::VisualEffect(mDevice);
		mVisualEffect->initUniformManager(
			mSwapChain->getImageCount(),
			mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods)->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler);
		mVisualEffect->RecordingCommandBuffer(mRenderPass, mSwapChain, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods));

		// 创建多人玩家
		mCrowd = new Crowd(100, mDevice, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods), mSwapChain, mRenderPass, mSampler, mCameraVPMatricesBuffer, mDungeon);
		mCrowd->SteSquarePhysics(mSquarePhysics);
		mCrowd->SetArms(mArms);

		// 创建玩家
		mGamePlayer = new GamePlayer(mDevice, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods), mSwapChain, mRenderPass, mSquarePhysics, mGamePlayerPosX, mGamePlayerPosY);
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
		Opcode::OpCrowd = mCrowd;
		Opcode::OpGamePlayer = mGamePlayer;
		Opcode::OpImGuiInterFace = InterFace;

		mCrowd->AddNPC(-208, 60);
	}

	UnlimitednessMapMods::~UnlimitednessMapMods()
	{
		LOGD("UnlimitednessMapMods::~UnlimitednessMapMods destructor");
		delete JPSPathfinding;
		delete AStarPathfinding;
		delete mAuxiliaryVision;
		delete mCrowd;          // 必须在 mDungeon 之前：NPC 的 JPS 线程使用 Dungeon 数据
		delete mGamePlayer;     // 必须在 mDungeon 之前：GamePlayer 析构时 mSquarePhysics 仍需要有效
		delete mArms;           // 可能调用 mSquarePhysics->RemoveObject/GetMapFormwork
		mArms = nullptr;
		delete mDungeon;        // 析构时 SetMapFormwork(nullptr)，需要 mSquarePhysics
		delete mSquarePhysics;  // 最后销毁物理世界
		mSquarePhysics = nullptr;
		delete mVisualEffect;
		delete mDamagePrompt;
	}

	// 鼠标移动事件
	void UnlimitednessMapMods::MouseMove(double xpos, double ypos)
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

	void UnlimitednessMapMods::MouseButton(MouseBtn button, InputState State)
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

	// 鼠标移动事件
	void UnlimitednessMapMods::MouseRoller(int z)
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

	// 键盘事件
	void UnlimitednessMapMods::KeyDown(GameKeyEnum moveDirection)
	{
		switch (moveDirection)
		{
		case GameKeyEnum::MOVE_LEFT:
			PlayerForce.x -= 1;
			break;
		case GameKeyEnum::MOVE_RIGHT:
			PlayerForce.x += 1;
			break;
		case GameKeyEnum::MOVE_FRONT:
			PlayerForce.y += 1;
			break;
		case GameKeyEnum::MOVE_BACK:
			PlayerForce.y -= 1;
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

	void UnlimitednessMapMods::GameCommandBuffers(unsigned int Format_i)
	{
		// VP 矩阵写入（方案B：在 fence 等待之后用 imageIndex 写入，
		// 与 descriptor set 读取的索引一致，消除 mCurrentFrame != imageIndex 的撕裂）
		VPMatrices *mVPMatrices = (VPMatrices *)mCameraVPMatricesBuffer[Format_i]->getPersistentMappedPtr();
		mVPMatrices->mViewMatrix = mCamera->getViewMatrix();

		mDungeon->GetCommandBuffer(wThreadCommandBufferS, Format_i);
		mDungeon->GetGIFCommandBuffer(wThreadCommandBufferS, Format_i);

		mParticleSystem->GetCommandBuffer(wThreadCommandBufferS, Format_i);
		// 加到显示数组中
		mCrowd->GetCommandBufferS(wThreadCommandBufferS, Format_i);

		if (Global::MistSwitch)
		{
			mDungeon->GetMistCommandBuffer(wThreadCommandBufferS, Format_i);
		}

		mVisualEffect->GetCommandBuffer(wThreadCommandBufferS, Format_i);

		wThreadCommandBufferS->push_back(mGamePlayer->getCommandBuffer(Format_i));

		mAuxiliaryVision->GetCommandBuffer(wThreadCommandBufferS, Format_i);

		mDamagePrompt->GetCommandBuffer(wThreadCommandBufferS, Format_i);
	}

	void UnlimitednessMapMods::GameUI()
	{
		mPhysicsDebug.drawUI();
	}

	void UnlimitednessMapMods::GameLoop(unsigned int mCurrentFrame)
	{
		mCrowd->NPCEvent(mCurrentFrame, TOOL::FPStime);

		Global::GamePlayerX = mGamePlayer->GetObjectCollision()->pos.x;
		Global::GamePlayerY = mGamePlayer->GetObjectCollision()->pos.y;

		mDamagePrompt->UpDataDamagePrompt(Global::GamePlayerX, Global::GamePlayerY, TOOL::FPStime);

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

		mVisualEffect->SetPos(((int(huoqdedian.x) / 16) + (huoqdedian.x < 0 ? -1 : 0)) * 16 + 8, ((int(huoqdedian.y) / 16) + (huoqdedian.y < 0 ? -1 : 0)) * 16 + 8, 0, mCurrentFrame);

	mGamePlayer->GetObjectCollision()->angle = m_angle; // 设置玩家物理角度
	mGamePlayer->GetMovement()->SetMoveInput(PlayerForce);     // 方案E：方向投票 → 目标速度
	mGamePlayer->GetMovement()->SetLookAngle(m_angle);         // 朝向平滑跟随（消除瞬切）
	mGamePlayer->GetMovement()->Update(TOOL::FPStime);         // 在物理积分前施力
	PlayerForce = {0, 0};

		// 先推进动态地图板块位置，便于物理辅助显示的脏检测在同一帧生效
		MovePlateInfo LMovePlateInfo = mDungeon->UpPos(mGamePlayer->GetObjectCollision()->pos.x, mGamePlayer->GetObjectCollision()->pos.y);
		if (LMovePlateInfo.UpData)
		{
			mDungeon->UpdataMistData(LMovePlateInfo.X, LMovePlateInfo.Y);
			Global::MainCommandBufferUpdateRequest();
		}

		mPhysicsDebug.refreshMap(); // 地图轮廓（静态缓冲，必须在 Begin() 之前；动态板块移动会自动重绘）

		mAuxiliaryVision->Begin();
		TOOL::mTimer->StartTiming(u8"物理模拟 ", true);
		mSquarePhysics->PhysicsEmulator(TOOL::FPStime); // 物理事件
		TOOL::mTimer->StartEnd();

		// 渲染物理世界辅助视觉（物体本体 + 关节 + 碰撞 + 触发器；辅助开关打开后含位置/速度/受力等）
		mPhysicsDebug.drawWorld();

		m_angle = mGamePlayer->GetObjectCollision()->angle;

		mGamePlayer->UpData();												// 更新玩家伤痕
		mCamera->setCameraPos(mGamePlayer->GetObjectCollision()->pos); // 设置玩家位置

		mParticlesSpecialEffect->SpecialEffectsEvent(mCurrentFrame, TOOL::FPStime);

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
			mVisualEffect->SetPosAngle(angleObj->pos.x, angleObj->pos.y, 0, angleObj->angle, mCurrentFrame);
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
				// mGamePlayer->GetObjectCollision()->SetPos(mGamePlayer->GetObjectCollision()->GetPos() + glm::vec2{ 16 , -16});
				AStarPath.clear();
				JPSPath.clear();
				TOOL::mTimer->MomentTiming("AStar寻路耗时");
				AStarPathfinding->FindPath({beang.x, beang.y}, {end.x, end.y}, &AStarPath, {mDungeon->PathfindingDecoratorDeviationX, mDungeon->PathfindingDecoratorDeviationY});
				TOOL::mTimer->MomentEnd();
				TOOL::mTimer->MomentTiming("JPS寻路耗时");
				JPSPathfinding->FindPath({beang.x, beang.y}, {end.x, end.y}, &JPSPath, {mDungeon->PathfindingDecoratorDeviationX, mDungeon->PathfindingDecoratorDeviationY});
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
		static float UpDataGIFTime = 0;
		UpDataGIFTime += TOOL::FPStime;
		if (UpDataGIFTime > 0.1f)
		{
			UpDataGIFTime = 0;
			mDungeon->UpDataGIF();
		}

		// 战争迷雾
		TOOL::mTimer->StartTiming(u8"战争迷雾耗时 ", true);
		MistContinuityFire += TOOL::FPStime;
		if (MistContinuityFire > 0.03f && Global::MistSwitch)
		{
			MistContinuityFire = 0;
			mDungeon->UpdataMist(int(mCamera->getCameraPos().x), int(mCamera->getCameraPos().y), m_angle + 0.7853981633975f - 1.57f);
		}
		TOOL::mTimer->StartEnd();

		mAuxiliaryVision->End();

		// 请求每帧重录主指令缓冲，确保 GameCommandBuffers 每帧被调用以写入 VP 矩阵
		Global::MainCommandBufferUpdateRequest();
	}

	void UnlimitednessMapMods::GameRecordCommandBuffers()
	{
		mAuxiliaryVision->initCommandBuffer();
		mGamePlayer->InitCommandBuffer();
		mCrowd->ReconfigurationCommandBuffer();
		mVisualEffect->initCommandBuffer();
		mDungeon->initCommandBuffer();
		mDamagePrompt->initCommandBuffer();
	}

	// 游戏停止界面循环
	void UnlimitednessMapMods::GameStopInterfaceLoop(unsigned int mCurrentFrame)
	{
	}

	// 游戏 TCP事件
	void UnlimitednessMapMods::GameTCPLoop()
	{
	}
}
