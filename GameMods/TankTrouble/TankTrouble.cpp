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

		// 生成迷宫
		mLabyrinth = new FixedMaze(mSquarePhysics);
		mLabyrinth->InitLabyrinth(mDevice, 21, 21);

		// 绑定物理辅助显示（地图轮廓此时已注册进物理世界）
		mPhysicsDebug.setup(mAuxiliaryVision, mSquarePhysics);

		// 地形被子弹破坏时，标记地图轮廓为脏，下一帧 refreshMap() 重绘
		mLabyrinth->mTerrainChangedHandler = [this] { mPhysicsDebug.mapDirty = true; };

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

		// 接线 Arms ↔ 坦克：让子弹击中坦克时能正确销毁、爆炸能伤害到坦克
		mArms->SetPlayerTank(mGamePlayer);
		mArms->SetCrowd(mCrowd);
		mArms->RegisterTankBulletHandler(mGamePlayer);
		mArms->SetArmsMode(AttackModeEnum::Pistol); // 默认手枪（反弹 3 次）

		// 确保玩家碰撞回调在 mSquarePhysics 地图设置完成后生效
		// （GamePlayer 构造函数已调用，此处显式再注册一次以确保与 Arms 绑定一致）
		mGamePlayer->RegisterBulletHitCallback();

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

		// NPC 被击杀时计分（玩家击杀）
		mCrowd->mOnNPCKilled = [this] {
			if (mGameEnded) return;
			mPlayerKills++;
			LOGI("[TankTrouble] NPC killed. Player kills: %d / %d", mPlayerKills, mKillTarget);
			if (mPlayerKills >= mKillTarget) {
				mGameEnded = true;
				mPlayerWon = true;
			}
		};

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
		delete mLabyrinth;      // 析构时 SetMapFormwork(nullptr)，需要 mSquarePhysics
		delete mArms;           // 可能调用 mSquarePhysics->RemoveObject/GetMapFormwork
		mArms = nullptr;
		delete mSquarePhysics;  // 最后销毁物理世界
		mSquarePhysics = nullptr;
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
			// 切换到上一把武器（循环 0..3）
			AttackType = (AttackType == 0) ? (AttackModeEnumNumber) : (AttackType - 1);
			UpdateWeaponMode();
			break;
		case GameKeyEnum::Key_2:
			// 切换到下一把武器（循环 0..3）
			AttackType = (AttackType >= (unsigned int)AttackModeEnumNumber) ? 0 : (AttackType + 1);
			UpdateWeaponMode();
			break;
		default:
			break;
		}
	}

	// 把 AttackType 同步到 Arms 的当前武器配置（反弹/爆炸/射速）
	void TankTrouble::UpdateWeaponMode() {
		if (AttackType > (unsigned int)AttackModeEnumNumber) {
			AttackType = 0;
		}
		mArms->SetArmsMode(static_cast<AttackModeEnum>(AttackType));
	}

	// 游戏规则：玩家死亡重生、NPC 维持数量、胜负判定
	void TankTrouble::UpdateGameRules(float time) {
		// 游戏已结束则不再处理规则
		if (mGameEnded) return;

		// 玩家死亡重生流程
		if (mPlayerDying) {
			mRespawnTimer -= time;
			if (mRespawnTimer <= 0.0f) {
				mPlayerDying = false;
				// 在合法位置重生
				glm::ivec2 pos = mLabyrinth->GetLegitimateGeneratePos();
				mGamePlayer->ResetTank(pos.x, pos.y);
				mCamera->setCameraPos(mGamePlayer->GetObjectCollision()->pos);
			}
			return;
		}

		// 检测玩家是否死亡
		if (mGamePlayer->GetDeathInBattle()) {
			mPlayerDying = true;
			mPlayerDeaths++;
			mRespawnTimer = mRespawnDelay;
			LOGI("[TankTrouble] Player died. Deaths: %d", mPlayerDeaths);
			// 死亡数过多判定失败（死亡 3 次失败）
			if (mPlayerDeaths >= 3) {
				mGameEnded = true;
				mPlayerWon = false;
			}
			return;
		}

		// 维持至少 1 个 NPC 敌人（NPC 被杀光后自动补充）
		if (mCrowd && mCrowd->GetNPCCount() == 0) {
			glm::ivec2 pos = mLabyrinth->GetLegitimateGeneratePos();
			mCrowd->RespawnNPC(pos.x, pos.y);
		}
	}

	void TankTrouble::GameCommandBuffers(unsigned int Format_i)
	{
		// VP 矩阵写入（方案B：在 fence 等待之后用 imageIndex 写入，
		// 与 descriptor set 读取的索引一致，消除 mCurrentFrame != imageIndex 的撕裂）
		VPMatrices *mVPMatrices = (VPMatrices *)mCameraVPMatricesBuffer[Format_i]->getPersistentMappedPtr();
		mVPMatrices->mViewMatrix = mCamera->getViewMatrix();

		mLabyrinth->GetCommandBuffer(wThreadCommandBufferS, Format_i);

		mParticleSystem->GetCommandBuffer(wThreadCommandBufferS, Format_i);
		// 加到显示数组中
		mCrowd->GetCommandBufferS(wThreadCommandBufferS, Format_i);

		wThreadCommandBufferS->push_back(mGamePlayer->getCommandBuffer(Format_i));

		mAuxiliaryVision->GetCommandBuffer(wThreadCommandBufferS, Format_i);

		mDamagePrompt->GetCommandBuffer(wThreadCommandBufferS, Format_i);

		mUVDynamicDiagram->GetCommandBuffer(wThreadCommandBufferS, Format_i);
	}

	void TankTrouble::GameUI()
	{
		mPhysicsDebug.drawUI();

		// ===== HUD：武器、击杀、死亡 =====
		// 用 ImGui 在屏幕左上角显示状态
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowBgAlpha(0.6f);
		if (ImGui::Begin("TankTroubleHUD", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoInputs))
		{
			// 当前武器名
			const char* weaponName = "手枪";
			switch (mArms->GetCurrentWeapon()) {
				case AttackModeEnum::Pistol:     weaponName = "手枪 [反弹]"; break;
				case AttackModeEnum::Shrapnel:   weaponName = "霰弹"; break;
				case AttackModeEnum::MachineGun: weaponName = "机枪"; break;
				case AttackModeEnum::Rocket:     weaponName = "火箭 [爆炸]"; break;
			}
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), u8"武器: %s", weaponName);
			ImGui::Text(u8"击杀: %d / %d", mPlayerKills, mKillTarget);
			ImGui::Text(u8"死亡: %d / 3", mPlayerDeaths);
			ImGui::Separator();
			ImGui::TextDisabled(u8"[1/2] 切换武器  [WASD] 移动  [鼠标] 瞄准/射击");
		}
		ImGui::End();

		// ===== 游戏结束结算面板 =====
		if (mGameEnded) {
			ImGui::OpenPopup("TankTroubleGameOver");
		}
		ImGui::SetNextWindowSize(ImVec2(360, 180), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(mWinWidth / 2.0f - 180, mWinHeight / 2.0f - 90), ImGuiCond_FirstUseEver);
		if (ImGui::BeginPopupModal("TankTroubleGameOver", nullptr, ImGuiWindowFlags_NoResize))
		{
			if (mPlayerWon) {
				ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), u8"胜利！");
			} else {
				ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), u8"失败！");
			}
			ImGui::Separator();
			ImGui::Text(u8"击杀数: %d", mPlayerKills);
			ImGui::Text(u8"死亡数: %d", mPlayerDeaths);
			ImGui::Separator();
			if (ImGui::Button(u8"重新开始", ImVec2(-1, 0)))
			{
				// 重置游戏状态
				mPlayerKills = 0;
				mPlayerDeaths = 0;
				mGameEnded = false;
				mPlayerWon = false;
				mPlayerDying = false;
				mRespawnTimer = 0.0f;
				// 重生玩家
				glm::ivec2 pos = mLabyrinth->GetLegitimateGeneratePos();
				mGamePlayer->ResetTank(pos.x, pos.y);
				// 清空所有 NPC 并重新生成
				mCrowd->KillAll();
				for (int i = 0; i < 2; ++i) {
					glm::ivec2 npos = mLabyrinth->GetLegitimateGeneratePos();
					mCrowd->AddNPC(npos.x, npos.y);
				}
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	void TankTrouble::GameLoop(unsigned int mCurrentFrame)
	{
		mPhysicsDebug.refreshMap(); // 地图轮廓（静态缓冲，必须在 Begin() 之前）

		mAuxiliaryVision->Begin();

		Global::GamePlayerX = mGamePlayer->GetObjectCollision()->pos.x;
		Global::GamePlayerY = mGamePlayer->GetObjectCollision()->pos.y;

		mDamagePrompt->UpDataDamagePrompt(Global::GamePlayerX, Global::GamePlayerY, TOOL::FPStime);

		// 注意：原 IndexAnimationGrid 动画网格轮廓显示已随旧引擎移除（属渲染层重构，与玩法无关）
		if (!Global::ServerOrClient)
		{
			mCrowd->NPCTimeoutDetection();
		}
		else
		{
			mCrowd->NPCEvent(mCurrentFrame, TOOL::FPStime);
		}

		// 游戏规则：胜负判定 + 玩家/NPC 重生（在 NPC 死亡计数之后）
		UpdateGameRules(TOOL::FPStime);

		// 游戏结束后冻结输入与射击（仅渲染与 UI）
		if (mGameEnded) {
			mLeftMouseDown = false;
			PlayerForce = {0, 0};
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
	mGamePlayer->GetMovement()->SetMoveInput(PlayerForce);     // 方案E：方向投票 → 目标速度
	mGamePlayer->GetMovement()->SetLookAngle(m_angle);         // 朝向平滑跟随（消除瞬切）
	mGamePlayer->GetMovement()->Update(TOOL::FPStime);         // 在物理积分前施力
	PlayerForce = {0, 0};
	TOOL::mTimer->StartTiming(u8"物理模拟 ", true);
	mSquarePhysics->PhysicsEmulator(TOOL::FPStime); // 物理事件
	TOOL::mTimer->StartEnd();

		// 渲染物理世界辅助视觉（物体本体 + 关节 + 碰撞 + 触发器；辅助开关打开后含位置/速度/受力等）
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

		// 请求每帧重录主指令缓冲，确保 GameCommandBuffers 每帧被调用以写入 VP 矩阵
		Global::MainCommandBufferUpdateRequest();
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
