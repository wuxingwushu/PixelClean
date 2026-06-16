#include "MazeMods.h"
#include "../../NetworkTCP/ReplicationManager.h"
#include "../../NetworkTCP/NetworkLayer.h"
#include "MazeReplicationComponents.h"
#include "MazeReplicationEvents.h"
#include "../../Opcode/OpcodeFunction.h"
// DestroyMode now in game layer (old: #include "../../Physics/DestroyMode.h")
#include "../../DebugLog.h"

namespace GAME
{

	bool LabyrinthGetWallD(int x, int y, void *P)
	{
		Labyrinth *LLabyrinth = (Labyrinth *)P;
		return LLabyrinth->GetPixelWallNumber(x, y);
	}

	MazeMods::MazeMods(Configuration wConfiguration) : Configuration{wConfiguration}
	{
		LOGD("MazeMods::MazeMods constructor");

		// 创建模式私有物理世界
		mSquarePhysics = new PhysicsBlock::PhysicsWorld(Vec2_{0, 0}, false);
		// 创建模式私有武器系统
		mArms = new Arms(mParticleSystem, mParticleCount);
		mArms->SetSpecialEffect(mParticlesSpecialEffect);
		mArms->SetSquarePhysics(mSquarePhysics);

		mAuxiliaryVision = new VulKan::AuxiliaryVision(mDevice, mPipelineS, 50000);
		mAuxiliaryVision->initUniformManager(
			mSwapChain->getImageCount(),
			mCameraVPMatricesBuffer);
		mAuxiliaryVision->RecordingCommandBuffer(mRenderPass, mSwapChain);

		mDamagePrompt = new DamagePrompt(mDevice, mPipelineS->GetPipeline(VulKan::PipelineMods::DamagePrompt)->DescriptorSetLayout, mCameraVPMatricesBuffer,
										 mTextureLibrary->GetTextureUV("DamagePrompt").mTexture, mSwapChain->getImageCount(), 10);
		mDamagePrompt->RecordingCommandBuffer(mRenderPass, mSwapChain, mPipelineS->GetPipeline(VulKan::PipelineMods::DamagePrompt));

		mUVDynamicDiagram = new UVDynamicDiagram(mDevice, mPipelineS->GetPipeline(VulKan::PipelineMods::UVDynamicDiagram), mSwapChain, mRenderPass, mCameraVPMatricesBuffer, mSquarePhysics);
		mUVDynamicDiagram->InitCommandBuffer();
		// 测试寻路
		JPSPathfinding = new JPS(300, 10000);
		AStarPathfinding = new AStar(300, 10000);

		// 生成迷宫
		mLabyrinth = new Labyrinth(mSquarePhysics);
		mLabyrinth->InitLabyrinth(mDevice, 21, 21);

		// 绑定物理辅助显示（地图轮廓此时已注册进物理世界）
		mPhysicsDebug.setup(mAuxiliaryVision, mSquarePhysics);

		// 地形被子弹破坏时，标记地图轮廓为脏，下一帧 refreshMap() 重绘
		mLabyrinth->mTerrainChangedHandler = [this] { mPhysicsDebug.mapDirty = true; };

		if (!(Global::MultiplePeopleMode && !Global::ServerOrClient)) {
			mLabyrinth->initUniformManager(
				mDevice,
				mSwapChain->getImageCount(),
				mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods)->DescriptorSetLayout,
				&mCameraVPMatricesBuffer,
				mSampler);
			mLabyrinth->RecordingCommandBuffer(mRenderPass, mSwapChain, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods));
			mLabyrinthVulkanReady = true;
		}

		JPSPathfinding->SetObstaclesCallback(LabyrinthGetWallD, mLabyrinth);
		AStarPathfinding->SetObstaclesCallback(LabyrinthGetWallD, mLabyrinth);

		glm::ivec2 Lpos = mLabyrinth->GetLegitimateGeneratePos();

		mVisualEffect = new VulKan::VisualEffect(mDevice);
		mVisualEffect->initUniformManager(
			mSwapChain->getImageCount(),
			mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods)->DescriptorSetLayout,
			mCameraVPMatricesBuffer,
			mSampler);
		mVisualEffect->RecordingCommandBuffer(mRenderPass, mSwapChain, mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods));

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

		// === 新 Replication 系统初始化 ===
		if (Global::MultiplePeopleMode)
		{
			auto& netLayer = NetworkLayer::Get();
			NetworkRole role = Global::ServerOrClient
				? NetworkRole::Server : NetworkRole::Client;
			netLayer.Initialize(role, Global::ClientIP,
								Global::ServerPort, Global::ClientPort);

			auto& repMgr = ReplicationManager::Get();
			repMgr.RegisterRemoteComponentType(
				PlayerPositionComponent::kTypeId,
				[]() -> std::unique_ptr<ReplicableComponent> {
					return std::make_unique<PlayerPositionComponent>();
				});
			repMgr.RegisterRemoteComponentType(
				PlayerBrokenComponent::kTypeId,
				[]() -> std::unique_ptr<ReplicableComponent> {
					return std::make_unique<PlayerBrokenComponent>();
				});

			repMgr.SetSendCallback([&netLayer](
				evutil_socket_t targetFd,
				const uint8_t* data, size_t size) {
				netLayer.SendWithHeader(targetFd, 0, data, size);
			});
			repMgr.SetEventSendCallback([&netLayer](
				evutil_socket_t targetFd,
				const uint8_t* data, size_t size) {
				netLayer.SendWithHeader(targetFd, 1, data, size);
			});

			evutil_socket_t myId = Global::ServerOrClient ? 0 : static_cast<evutil_socket_t>(1);

			mLocalPlayerObj = repMgr.RegisterLocalObject(myId);
			mPlayerPosComp = mLocalPlayerObj->AddComponent<PlayerPositionComponent>();
			mPlayerBrokenComp = mLocalPlayerObj->AddComponent<PlayerBrokenComponent>();

			repMgr.RegisterEventHandler(BulletShootEvent::kTypeId,
				[this](evutil_socket_t sender, ByteReader& reader) {
					BulletShootEvent evt;
					evt.Deserialize(reader);
					mArms->ShootBullets(evt.x, evt.y, evt.angle, 500, evt.bulletType);
					if (Global::ServerOrClient)
					{
						ReplicationManager::Get().SendEvent(
							static_cast<evutil_socket_t>(-1),
							new BulletShootEvent(evt.x, evt.y, evt.angle, evt.bulletType));
					}
				});

			repMgr.RegisterEventHandler(PixelDestroyEvent::kTypeId,
				[this](evutil_socket_t sender, ByteReader& reader) {
					PixelDestroyEvent evt;
					evt.Deserialize(reader);
					mLabyrinth->mPixelQueue->add(evt.pixel);
					if (Global::ServerOrClient)
					{
						ReplicationManager::Get().SendEvent(
							static_cast<evutil_socket_t>(-1),
							new PixelDestroyEvent(evt.pixel));
					}
				});

			repMgr.RegisterEventHandler(ChatEvent::kTypeId,
				[this](evutil_socket_t sender, ByteReader& reader) {
					ChatEvent evt;
					evt.Deserialize(reader);
					InterFace->mChatBoxStr->add({evt.message, std::chrono::steady_clock::now()});
					if (Global::ServerOrClient)
					{
						ReplicationManager::Get().SendEvent(
							static_cast<evutil_socket_t>(-1),
							new ChatEvent(evt.message));
					}
				});

			if (!Global::ServerOrClient) {
				repMgr.RegisterEventHandler(LabyrinthInitDataEvent::kTypeId,
					[this](evutil_socket_t, ByteReader& reader) {
						auto evt = std::make_unique<LabyrinthInitDataEvent>();
						evt->Deserialize(reader);
						mPendingLabyrinthInit = std::move(evt);
					});
			}

			if (Global::ServerOrClient) {
				repMgr.RegisterEventHandler(RequestLabyrinthInitEvent::kTypeId,
					[this](evutil_socket_t senderFd, ByteReader&) {
						auto* lab = mLabyrinth;
						auto* evt = new LabyrinthInitDataEvent();
						evt->numberX = lab->numberX;
						evt->numberY = lab->numberY;
						evt->blockTypesData.resize(lab->numberX * lab->numberY);
						for (int ix = 0; ix < lab->numberX; ++ix) {
							std::memcpy(&evt->blockTypesData[ix * lab->numberY],
										lab->BlockTypeS[ix],
										lab->numberY * sizeof(unsigned int));
						}
						evt->blockPixelsData.resize(lab->numberX * lab->numberY * 16 * 16);
						std::memcpy(evt->blockPixelsData.data(), lab->BlockPixelS,
									evt->blockPixelsData.size() * sizeof(int));

						ReplicationManager::Get().SendEvent(senderFd, evt);
					});
			}
		}

		// 给操作码对象赋值
		Opcode::OpArms = mArms;
		Opcode::OpLabyrinth = mLabyrinth;
		Opcode::OpCrowd = mCrowd;
		Opcode::OpGamePlayer = mGamePlayer;
		Opcode::OpImGuiInterFace = InterFace;

		if (!(Global::MultiplePeopleMode && !Global::ServerOrClient)) {
			mCrowd->AddNPC(-208, 60);
		}
	}

	MazeMods::~MazeMods()
	{
		LOGD("MazeMods::~MazeMods destructor");
		delete JPSPathfinding;
		delete AStarPathfinding;
		delete mAuxiliaryVision;
		delete mCrowd;          // 必须在 mLabyrinth 之前：NPC 的 JPS 线程使用 Labyrinth 的网格数据
		delete mGamePlayer;     // 必须在 mLabyrinth 之前：GamePlayer 析构时 mSquarePhysics 仍需要有效
		delete mLabyrinth;      // 析构时 SetMapFormwork(nullptr)，需要 mSquarePhysics
		delete mArms;           // 可能调用 mSquarePhysics->RemoveObject/GetMapFormwork
		mArms = nullptr;
		delete mSquarePhysics;  // 最后销毁物理世界
		mSquarePhysics = nullptr;
		delete mVisualEffect;
		delete mDamagePrompt;
		delete mUVDynamicDiagram;

		if (Global::MultiplePeopleMode)
		{
			NetworkLayer::Get().Shutdown();
			Global::MultiplePeopleMode = false;
		}
	}

	void MazeMods::MouseMove(double xpos, double ypos)
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

	void MazeMods::MouseButton(MouseBtn button, InputState State)
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

	void MazeMods::MouseRoller(int z)
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

	void MazeMods::KeyDown(GameKeyEnum moveDirection)
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

	void MazeMods::GameCommandBuffers(unsigned int Format_i)
	{
		// VP 矩阵写入（方案B：在 fence 等待之后用 imageIndex 写入，
		// 与 descriptor set 读取的索引一致，消除 mCurrentFrame != imageIndex 的撕裂）
		VPMatrices *mVPMatrices = (VPMatrices *)mCameraVPMatricesBuffer[Format_i]->getPersistentMappedPtr();
		mVPMatrices->mViewMatrix = mCamera->getViewMatrix();

		if (mLabyrinthVulkanReady) {
			mLabyrinth->GetCommandBuffer(wThreadCommandBufferS, Format_i);
		}

		mParticleSystem->GetCommandBuffer(wThreadCommandBufferS, Format_i);
		// 加到显示数组中
		mCrowd->GetCommandBufferS(wThreadCommandBufferS, Format_i);

		if (Global::MistSwitch && mLabyrinthVulkanReady)
		{
			mLabyrinth->GetMistCommandBuffer(wThreadCommandBufferS, Format_i);
		}

		mVisualEffect->GetCommandBuffer(wThreadCommandBufferS, Format_i);

		wThreadCommandBufferS->push_back(mGamePlayer->getCommandBuffer(Format_i));

		mAuxiliaryVision->GetCommandBuffer(wThreadCommandBufferS, Format_i);

		mDamagePrompt->GetCommandBuffer(wThreadCommandBufferS, Format_i);

		mUVDynamicDiagram->GetCommandBuffer(wThreadCommandBufferS, Format_i);
	}

	void MazeMods::GameUI()
	{
		mPhysicsDebug.drawUI();
	}

	void MazeMods::GameLoop(unsigned int mCurrentFrame)
	{
		if (mPendingLabyrinthInit) {
			auto& d = *mPendingLabyrinthInit;
			while (mLabyrinth->mPixelQueue->GetNumber() != 0) {
				delete mLabyrinth->mPixelQueue->pop();
			}
			vkDeviceWaitIdle(mDevice->getDevice());
			mLabyrinth->LoadLabyrinth(d.numberX, d.numberY,
				d.blockPixelsData.data(), d.blockTypesData.data(),
				mRenderPass, mSwapChain,
				mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods),
				&mCameraVPMatricesBuffer, mSampler);
			mArms->GetSquarePhysics()->SetMapFormwork(
				mLabyrinth->mFixedSizeTerrain);
			mLabyrinthVulkanReady = true;
			mPendingLabyrinthInit.reset();
			mJustLoadedLabyrinth = true;
			mPhysicsDebug.mapDirty = true; // 地图内容已变，下帧 refreshMap() 重写静态缓冲
		}

		mPhysicsDebug.refreshMap(); // 地图轮廓（静态缓冲，必须在 Begin() 之前）

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
				0.25f,
				glm::vec4{1, 0, 0, 1});
		}
		*/

		/*for (size_t i = 0; i < mGamePlayer->GetObjectCollision()->GetOutlinePointSize(); i++)
		{
			mAuxiliaryVision->AddSpot(
				{
					PhysicsBlock::vec2angle(
						mGamePlayer->GetObjectCollision()->GetOutlinePointSet(i),
						mGamePlayer->GetObjectCollision()->GetAngle()
					) + glm::dvec2(mGamePlayer->GetObjectCollision()->GetPos()),
					0
				},
				glm::vec4{ 0,0,1,1 }
			);
		}*/

		// TODO: AnimationEvent temporarily disabled - IndexAnimationGrid is old engine concept
		// mUVDynamicDiagram->AnimationEvent(TOOL::FPStime);
		if (!Global::ServerOrClient)
		{
			vkDeviceWaitIdle(mDevice->getDevice());
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

		mVisualEffect->SetPos(((int(huoqdedian.x) / 16) + (huoqdedian.x < 0 ? -1 : 0)) * 16 + 8, ((int(huoqdedian.y) / 16) + (huoqdedian.y < 0 ? -1 : 0)) * 16 + 8, 0, mCurrentFrame);

	mGamePlayer->GetObjectCollision()->angle = m_angle; // 设置玩家物理角度
	mGamePlayer->GetMovement()->SetMoveInput(PlayerForce);     // 方案E：方向投票 → 目标速度
	mGamePlayer->GetMovement()->SetLookAngle(m_angle);         // 朝向平滑跟随（消除瞬切）
	mGamePlayer->GetMovement()->Update(TOOL::FPStime);         // 在物理积分前施力
	PlayerForce = {0, 0};
	TOOL::mTimer->StartTiming(u8"物理模拟 ", true);
	mSquarePhysics->PhysicsEmulator(TOOL::FPStime); // 物理事件
	TOOL::mTimer->StartEnd();

		// 渲染物理世界辅助视觉（物体本体 + 关节 + 碰撞 + 触发器；辅助开关打开后含位置/速度/受力等）
		mPhysicsDebug.drawWorld();

		m_angle = mGamePlayer->GetObjectCollision()->angle;

		mGamePlayer->UpData();												// 更新玩家伤痕
		mCamera->setCameraPos(mGamePlayer->GetObjectCollision()->pos); // 设置玩家位置

		if (Global::MultiplePeopleMode && mPlayerPosComp) {
			mPlayerPosComp->x.Set(mGamePlayer->GetObjectCollision()->pos.x);
			mPlayerPosComp->y.Set(mGamePlayer->GetObjectCollision()->pos.y);
			mPlayerPosComp->angle.Set(m_angle);
			mPlayerPosComp->ForceAllDirty();
		}

		if (Global::MultiplePeopleMode) {
			auto& repMgr = ReplicationManager::Get();
			repMgr.ForEachRemoteObject([this, mCurrentFrame](ReplicableObject* obj) {
				evutil_socket_t key = obj->GetNetworkId();
				GamePlayer* player = mCrowd->GetGamePlayer(key);
				if (!player) return;
				auto* posComp = static_cast<PlayerPositionComponent*>(
					obj->GetComponentByTypeId(PlayerPositionComponent::kTypeId));
				if (posComp) {
					auto* collision = player->GetObjectCollision();
					collision->pos = Vec2_{static_cast<FLOAT_>(posComp->x.Get()),
									   static_cast<FLOAT_>(posComp->y.Get())};
					collision->angle = posComp->angle.Get();
					player->setGamePlayerMatrix(TOOL::FPStime, mCurrentFrame);
				}
			});
		}

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
					if (!Global::MultiplePeopleMode || Global::ServerOrClient)
					{
						mArms->Shoot(mCamera->getCameraPos().x + Armsdain.x, mCamera->getCameraPos().y + Armsdain.y, m_angle, 500, AttackType);
					}
					if (Global::MultiplePeopleMode)
					{
						ReplicationManager::Get().SendEvent(
							static_cast<evutil_socket_t>(-1),
							new BulletShootEvent(
								float(mCamera->getCameraPos().x + Armsdain.x),
								float(mCamera->getCameraPos().y + Armsdain.y),
								m_angle, AttackType));
					}
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
				AStarPath.clear();
				JPSPath.clear();

				TOOL::mTimer->MomentTiming("AStar寻路耗时");
				AStarPathfinding->FindPath({beang.x, beang.y}, {end.x, end.y}, &AStarPath);
				TOOL::mTimer->MomentEnd();
				TOOL::mTimer->MomentTiming("JPS寻路耗时");
				JPSPathfinding->FindPath({beang.x, beang.y}, {end.x, end.y}, &JPSPath);
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
		if (mJustLoadedLabyrinth) {
			MistContinuityFire = 0;
			mJustLoadedLabyrinth = false;
		}
		if (mLabyrinthVulkanReady) {
			mLabyrinth->UpDateMaps();
		}

		// 战争迷雾
		TOOL::mTimer->StartTiming(u8"战争迷雾耗时 ", true);
		MistContinuityFire += TOOL::FPStime;
		if (MistContinuityFire > 0.02f && Global::MistSwitch && mLabyrinthVulkanReady)
		{
			MistContinuityFire = 0;
			mLabyrinth->UpdataMist(int(mCamera->getCameraPos().x), int(mCamera->getCameraPos().y), m_angle + 0.7853981633975f - 1.57f);
		}
		TOOL::mTimer->StartEnd();

		mAuxiliaryVision->End();

		// 请求每帧重录主指令缓冲，确保 GameCommandBuffers 每帧被调用以写入 VP 矩阵
		Global::MainCommandBufferUpdateRequest();
	}

	void MazeMods::GameRecordCommandBuffers()
	{
		mAuxiliaryVision->initCommandBuffer();
		mGamePlayer->InitCommandBuffer();
		mCrowd->ReconfigurationCommandBuffer();
		mVisualEffect->initCommandBuffer();
		if (mLabyrinthVulkanReady) {
			mLabyrinth->ThreadUpdateCommandBuffer();
		}
		mDamagePrompt->initCommandBuffer();
		mUVDynamicDiagram->InitCommandBuffer();
	}

	// 游戏停止界面循环
	void MazeMods::GameStopInterfaceLoop(unsigned int mCurrentFrame)
	{
		if (InterFace->GetInterfaceIndexes() == InterFaceEnum_::ViceInterface_Enum && Global::MultiplePeopleMode && !Global::ServerOrClient)
		{
			mCrowd->UpTime();
		}
	}

	// 游戏 TCP事件
	void MazeMods::GameTCPLoop()
	{
		if (Global::MultiplePeopleMode)
		{
			NetworkLayer::Get().RunLoop();

			ReplicationManager::Get().Tick();

			ReplicationManager::Get().TickTimeouts(1000);

			if (!Global::ServerOrClient && !mInitRequested)
			{
				mInitRequested = true;
				ReplicationManager::Get().SendEvent(
					static_cast<evutil_socket_t>(-1),
					new RequestLabyrinthInitEvent());
			}
		}
	}

	void MazeMods::ProcessPendingLabyrinthInit()
	{
		if (!mPendingLabyrinthInit) return;
		auto& d = *mPendingLabyrinthInit;
		while (mLabyrinth->mPixelQueue->GetNumber() != 0) {
			delete mLabyrinth->mPixelQueue->pop();
		}
		vkDeviceWaitIdle(mDevice->getDevice());
		mLabyrinth->LoadLabyrinth(d.numberX, d.numberY,
			d.blockPixelsData.data(), d.blockTypesData.data(),
			mRenderPass, mSwapChain,
			mPipelineS->GetPipeline(VulKan::PipelineMods::MainMods),
			&mCameraVPMatricesBuffer, mSampler);
		mArms->GetSquarePhysics()->SetMapFormwork(
			mLabyrinth->mFixedSizeTerrain);
		mLabyrinthVulkanReady = true;
		mJustLoadedLabyrinth = true;
		mPendingLabyrinthInit.reset();
		mPhysicsDebug.mapDirty = true; // 地图内容已变，下帧 refreshMap() 重写静态缓冲
	}

}
