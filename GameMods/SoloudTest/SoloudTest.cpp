#include "SoloudTest.h"
#include "../../GlobalVariable.h"
#include "../../PhysicsBlock/BaseCalculate.hpp"
#include "../../NetworkTCP/Server.h"
#include "../../NetworkTCP/Client.h"

namespace GAME
{

	// ==================== 构造 / 析构 ====================

	SoloudTest::SoloudTest(Configuration wConfiguration) : Configuration{wConfiguration}
	{
		mAuxiliaryVision = new VulKan::AuxiliaryVision(mDevice, mPipelineS, 200000);
		mAuxiliaryVision->initUniformManager(
			mSwapChain->getImageCount(),
			mCameraVPMatricesBuffer);
		mAuxiliaryVision->RecordingCommandBuffer(mRenderPass, mSwapChain);

		mSoloud = SoundEffect::SoundEffect::GetSoundEffect()->GetSoloud();
		SoundEffect::SoundEffect::GetSoundEffect()->StopAll();

		CreateScene();
		InitAudio();

		mCamera->setCameraPos({0, 0, 40});
	}

	SoloudTest::~SoloudTest()
	{
		if (mSoloud) {
			mSoloud->stopAll();
		}
		mSoundSources.clear();
		delete mAuxiliaryVision;
		delete mPhysicsWorld;
	}

	// ==================== 场景创建 ====================
	//
	// 创建 64×40 的大型侧视图地图，包含：
	//   建筑群：A楼（左楼）+ B楼（右楼）+ 中央庭院
	//
	//   A楼 (左楼, 网格 x=[5,30] y=[4,27], 世界 x=[-27,-2] y=[-16,7])：
	//     ┌─────────────────────────┐
	//     │  音乐室A1  │  图书室A2  │  ← y=17~26 (上层)
	//     │            │            │
	//     ├────────────┤            │  ← y=16 地板
	//     │            │            │
	//     │  储藏室A3  │  工坊A4    │  ← y=9~15 (中层)
	//     │            │            │
	//     ├────────────┴────────────┤  ← y=8 地板
	//     │      下层走廊          │  ← y=5~7
	//     └─────────────────────────┘
	//
	//   B楼 (右楼, 网格 x=[34,58] y=[4,27], 世界 x=[2,26] y=[-16,7])：
	//     ┌─────────────────────────┐
	//     │  水泵房B1  │  实验室B2  │  ← y=13~26 (上层)
	//     │            │            │
	//     ├────────────┴────────────┤  ← y=12 地板
	//     │      下层大厅          │  ← y=5~11
	//     └─────────────────────────┘
	//
	//   庭院 (网格 x=[30,34] y=[4,15], 世界 x=[-2,2] y=[-16,-5])：
	//     连接两楼的露天庭院
	//
	// 声源分布 (7个)：
	//   1. 室外风声 — 地图左上角室外
	//   2. 音乐室旋律 — A1 音乐室内
	//   3. 水滴声 — B1 水泵房内
	//   4. 走廊嗡嗡声 — A楼下层走廊
	//   5. 机器轰鸣 — A4 工坊内
	//   6. 室外雨声 — 地图右上角室外
	//   7. 实验室电子音 — B2 实验室内

	void SoloudTest::CreateScene()
	{
		mPhysicsWorld = new PhysicsBlock::PhysicsWorld({0.0, 0.0}, false);
		mMapStatic = new PhysicsBlock::MapStatic(MAP_WIDTH, MAP_HEIGHT);

		for (int i = 0; i < MAP_WIDTH * MAP_HEIGHT; ++i) {
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
		}

		// ===== 四周边界墙 =====
		for (int i = 0; i < MAP_WIDTH; ++i) {
			mMapStatic->at(i, 0).Entity = true;
			mMapStatic->at(i, 0).Collision = true;
			mMapStatic->at(i, MAP_HEIGHT - 1).Entity = true;
			mMapStatic->at(i, MAP_HEIGHT - 1).Collision = true;
		}
		for (int i = 0; i < MAP_HEIGHT; ++i) {
			mMapStatic->at(0, i).Entity = true;
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MAP_WIDTH - 1, i).Entity = true;
			mMapStatic->at(MAP_WIDTH - 1, i).Collision = true;
		}

		// ===== A楼 (左楼) x=[5,30] y=[4,27] =====
		int ax1 = 5, ax2 = 30;
		int ay1 = 4, ay2 = 27;

		// A楼四壁
		for (int y = ay1; y <= ay2; ++y) {
			mMapStatic->at(ax1, y).Entity = true;
			mMapStatic->at(ax1, y).Collision = true;
			mMapStatic->at(ax2, y).Entity = true;
			mMapStatic->at(ax2, y).Collision = true;
		}
		for (int x = ax1; x <= ax2; ++x) {
			mMapStatic->at(x, ay1).Entity = true;
			mMapStatic->at(x, ay1).Collision = true;
			mMapStatic->at(x, ay2).Entity = true;
			mMapStatic->at(x, ay2).Collision = true;
		}

		// A楼 内部垂直隔墙 x=17, 门洞 y=8~10 (下层通道), y=17~19 (上层通道)
		int awx = 17;
		for (int y = ay1; y <= ay2; ++y) {
			if ((y >= 8 && y <= 10) || (y >= 17 && y <= 19)) continue;
			mMapStatic->at(awx, y).Entity = true;
			mMapStatic->at(awx, y).Collision = true;
		}

		// A楼 内部水平地板 y=8 (下层走廊天花板), 开口 x=10~12, x=23~25 (楼梯井)
		for (int x = ax1 + 1; x <= ax2 - 1; ++x) {
			if ((x >= 10 && x <= 12) || (x >= 23 && x <= 25)) continue;
			mMapStatic->at(x, 8).Entity = true;
			mMapStatic->at(x, 8).Collision = true;
		}

		// A楼 内部水平地板 y=16 (中层房间地板 / 上层走廊天花板), 开口 x=10~12, x=23~25
		for (int x = ax1 + 1; x <= ax2 - 1; ++x) {
			if ((x >= 10 && x <= 12) || (x >= 23 && x <= 25)) continue;
			mMapStatic->at(x, 16).Entity = true;
			mMapStatic->at(x, 16).Collision = true;
		}

		// A楼 左墙入口: x=5, y=5~6 (下层入口), y=18~19 (上层入口/外楼梯)
		mMapStatic->at(ax1, 5).Entity = false;
		mMapStatic->at(ax1, 5).Collision = false;
		mMapStatic->at(ax1, 6).Entity = false;
		mMapStatic->at(ax1, 6).Collision = false;
		mMapStatic->at(ax1, 18).Entity = false;
		mMapStatic->at(ax1, 18).Collision = false;
		mMapStatic->at(ax1, 19).Entity = false;
		mMapStatic->at(ax1, 19).Collision = false;

		// A楼 右墙出口到庭院: x=30, y=8~9
		mMapStatic->at(ax2, 8).Entity = false;
		mMapStatic->at(ax2, 8).Collision = false;
		mMapStatic->at(ax2, 9).Entity = false;
		mMapStatic->at(ax2, 9).Collision = false;

		// ===== B楼 (右楼) x=[34,58] y=[4,27] =====
		int bx1 = 34, bx2 = 58;
		int by1 = 4, by2 = 27;

		for (int y = by1; y <= by2; ++y) {
			mMapStatic->at(bx1, y).Entity = true;
			mMapStatic->at(bx1, y).Collision = true;
			mMapStatic->at(bx2, y).Entity = true;
			mMapStatic->at(bx2, y).Collision = true;
		}
		for (int x = bx1; x <= bx2; ++x) {
			mMapStatic->at(x, by1).Entity = true;
			mMapStatic->at(x, by1).Collision = true;
			mMapStatic->at(x, by2).Entity = true;
			mMapStatic->at(x, by2).Collision = true;
		}

		// B楼 内部垂直隔墙 x=46, 门洞 y=14~16
		int bwx = 46;
		for (int y = by1; y <= by2; ++y) {
			if (y >= 14 && y <= 16) continue;
			mMapStatic->at(bwx, y).Entity = true;
			mMapStatic->at(bwx, y).Collision = true;
		}

		// B楼 内部水平地板 y=12, 开口 x=41~43
		for (int x = bx1 + 1; x <= bx2 - 1; ++x) {
			if (x >= 41 && x <= 43) continue;
			mMapStatic->at(x, 12).Entity = true;
			mMapStatic->at(x, 12).Collision = true;
		}

		// B楼 左墙入口: x=34, y=5~6 (庭院进入下层大厅), y=17~18 (庭院进入上层B1)
		mMapStatic->at(bx1, 5).Entity = false;
		mMapStatic->at(bx1, 5).Collision = false;
		mMapStatic->at(bx1, 6).Entity = false;
		mMapStatic->at(bx1, 6).Collision = false;
		mMapStatic->at(bx1, 17).Entity = false;
		mMapStatic->at(bx1, 17).Collision = false;
		mMapStatic->at(bx1, 18).Entity = false;
		mMapStatic->at(bx1, 18).Collision = false;

		// ===== 庭院地面 (两楼之间) =====
		for (int x = 31; x <= 33; ++x) {
			mMapStatic->at(x, 4).Entity = true;
			mMapStatic->at(x, 4).Collision = true;
		}

		// ===== 室外装饰柱/树 =====
		// 左上角柱子群
		for (int y = 30; y <= 32; ++y) {
			mMapStatic->at(2, y).Entity = true;
			mMapStatic->at(2, y).Collision = true;
			mMapStatic->at(4, y).Entity = true;
			mMapStatic->at(4, y).Collision = true;
		}
		// 右上角柱子
		for (int y = 32; y <= 34; ++y) {
			mMapStatic->at(60, y).Entity = true;
			mMapStatic->at(60, y).Collision = true;
		}
		// 中央上方平台
		for (int x = 28; x <= 36; ++x) {
			mMapStatic->at(x, 34).Entity = true;
			mMapStatic->at(x, 34).Collision = true;
		}
		// 平台开口
		mMapStatic->at(31, 34).Entity = false;
		mMapStatic->at(31, 34).Collision = false;
		mMapStatic->at(32, 34).Entity = false;
		mMapStatic->at(32, 34).Collision = false;
		mMapStatic->at(33, 34).Entity = false;
		mMapStatic->at(33, 34).Collision = false;

		// 设置地图中心点
		mMapStatic->SetCentrality({MAP_WIDTH / 2.0, MAP_HEIGHT / 2.0});
		mPhysicsWorld->SetMapFormwork(mMapStatic);

		// ===== 渲染所有静态地图方块 =====
		Vec2_ centrality = mMapStatic->centrality;
		for (size_t x = 0; x < mMapStatic->width; ++x) {
			for (size_t y = 0; y < mMapStatic->height; ++y) {
				if (!mMapStatic->at(x, y).Entity) continue;

				Vec2_ worldPos = Vec2_{(FLOAT_)x, (FLOAT_)y} - centrality;
				glm::vec4 color = {0.15f, 0.7f, 0.15f, 1.0f};

				// 边界墙 → 暗灰色
				if (x == 0 || x == MAP_WIDTH - 1 || y == 0 || y == MAP_HEIGHT - 1) {
					color = {0.3f, 0.3f, 0.35f, 1.0f};
				}
				// A楼外墙 → 深绿
				else if (((x == ax1 || x == ax2) && y >= ay1 && y <= ay2) ||
					     ((y == ay1 || y == ay2) && x >= ax1 && x <= ax2)) {
					color = {0.15f, 0.6f, 0.2f, 1.0f};
				}
				// A楼内墙 → 浅绿
				else if ((x == awx && y >= ay1 && y <= ay2) || (y == 8 && x >= ax1 + 1 && x <= ax2 - 1) || (y == 16 && x >= ax1 + 1 && x <= ax2 - 1)) {
					color = {0.25f, 0.75f, 0.35f, 1.0f};
				}
				// B楼外墙 → 蓝绿
				else if (((x == bx1 || x == bx2) && y >= by1 && y <= by2) ||
					     ((y == by1 || y == by2) && x >= bx1 && x <= bx2)) {
					color = {0.15f, 0.55f, 0.55f, 1.0f};
				}
				// B楼内墙 → 青色
				else if ((x == bwx && y >= by1 && y <= by2) || (y == 12 && x >= bx1 + 1 && x <= bx2 - 1)) {
					color = {0.2f, 0.7f, 0.7f, 1.0f};
				}
				// 庭院地面/装饰 → 暖黄绿
				else if ((x >= 31 && x <= 33) && y == 4) {
					color = {0.4f, 0.65f, 0.2f, 1.0f};
				}
				// 装饰柱/平台 → 棕色
				else {
					color = {0.45f, 0.5f, 0.25f, 1.0f};
				}

				ShowStaticSquare(worldPos, 0, color);
			}
		}

		// ===== 物理对象 =====

		// 玩家 — 蓝色圆形，放在A楼左下外侧
		mPlayer = new PhysicsBlock::PhysicsCircle({-24, -15}, 0.55, 2.5, 0.3);
		mPhysicsWorld->AddObject(mPlayer);

		// ---- 装饰球 ----
		PhysicsBlock::PhysicsCircle *ball1 = new PhysicsBlock::PhysicsCircle({-18, -13}, 0.5, 1.5, 0.3);
		mPhysicsWorld->AddObject(ball1);

		PhysicsBlock::PhysicsCircle *ball2 = new PhysicsBlock::PhysicsCircle({-10, -5}, 0.35, 1.0, 0.3);
		mPhysicsWorld->AddObject(ball2);

		PhysicsBlock::PhysicsCircle *ball3 = new PhysicsBlock::PhysicsCircle({10, -14}, 0.6, 2.0, 0.3);
		mPhysicsWorld->AddObject(ball3);

		PhysicsBlock::PhysicsCircle *ball4 = new PhysicsBlock::PhysicsCircle({20, -12}, 0.45, 1.2, 0.3);
		mPhysicsWorld->AddObject(ball4);

		// ---- 可推动方块 ----
		// 方块1 — A4工坊内的大箱子
		PhysicsBlock::PhysicsShape *box1 = new PhysicsBlock::PhysicsShape({-10, -6}, {3, 2});
		for (size_t i = 0; i < box1->width * box1->height; ++i) {
			box1->at(i).Collision = true;
			box1->at(i).Entity = true;
			box1->at(i).mass = 1;
			box1->at(i).FrictionFactor = 0.3;
		}
		box1->UpdateAll();
		box1->angle = 0.2;
		mPhysicsWorld->AddObject(box1);

		// 方块2 — 庭院里的箱子
		PhysicsBlock::PhysicsShape *box2 = new PhysicsBlock::PhysicsShape({0, -14}, {2, 2});
		for (size_t i = 0; i < box2->width * box2->height; ++i) {
			box2->at(i).Collision = true;
			box2->at(i).Entity = true;
			box2->at(i).mass = 1;
			box2->at(i).FrictionFactor = 0.25;
		}
		box2->UpdateAll();
		mPhysicsWorld->AddObject(box2);

		// 方块3 — B楼大厅里的箱子
		PhysicsBlock::PhysicsShape *box3 = new PhysicsBlock::PhysicsShape({15, -13}, {2, 2});
		for (size_t i = 0; i < box3->width * box3->height; ++i) {
			box3->at(i).Collision = true;
			box3->at(i).Entity = true;
			box3->at(i).mass = 1;
			box3->at(i).FrictionFactor = 0.2;
		}
		box3->UpdateAll();
		mPhysicsWorld->AddObject(box3);

		// 方块4 — 高处平台上的物件
		PhysicsBlock::PhysicsShape *box4 = new PhysicsBlock::PhysicsShape({-1, 15}, {2, 1});
		for (size_t i = 0; i < box4->width * box4->height; ++i) {
			box4->at(i).Collision = true;
			box4->at(i).Entity = true;
			box4->at(i).mass = 1;
			box4->at(i).FrictionFactor = 0.4;
		}
		box4->UpdateAll();
		box4->angle = 0;
		mPhysicsWorld->AddObject(box4);
	}

	// ==================== 音频初始化 ====================
	//
	// 音频架构：
	//   SoLoud 引擎
	//   ├── Environment Bus（环境声总线）
	//   │   ├── Filter 0: FreeverbFilter（混响 — 室内时增大）
	//   │   ├── Filter 1: EchoFilter（回声 — 走廊时增大）
	//   │   ├── SfxrWind（室外风声）
	//   │   ├── SfxrWater（水泵房水滴声）
	//   │   ├── SfxrHum（走廊嗡嗡声）
	//   │   ├── SfxrMachine（工坊机器轰鸣）
	//   │   └── SfxrRain（室外雨声）
	//   ├── Music Bus（音乐总线）
	//   │   ├── Filter 0: BiquadResonantFilter LPF
	//   │   └── SfxrMusic（音乐室旋律）
	//   └── SFX Bus（音效总线）
	//       ├── Filter 0: LofiFilter（走廊LoFi — 保留）
	//       ├── SfxrFootstep（脚步声）
	//       ├── SfxrImpact（碰撞冲击声）
	//       ├── SfxrBlip（区域过渡提示音）
	//       └── SfxrLab（实验室电子音 → 走 SFX Bus）

	void SoloudTest::InitAudio()
	{
		// ---- 初始化滤波器 ----
		mReverbFilter.setParams(0, 0.5f, 0.5f, 1.0f);
		mOcclusionLPF.setParams(SoLoud::BiquadResonantFilter::LOWPASS, 22000, 1);
		mMusicLPF.setParams(SoLoud::BiquadResonantFilter::LOWPASS, 22000, 1);
		mEchoFilter.setParams(0.2f, 0.6f);
		mLofiFilter.setParams(8000, 8);

		// ---- 挂载滤波器到总线 ----
		mEnvironmentBus.setFilter(0, &mReverbFilter);
		mEnvironmentBus.setFilter(1, &mEchoFilter);
		mMusicBus.setFilter(0, &mMusicLPF);
		mSfxBus.setFilter(0, &mLofiFilter);

		// ---- 启动总线 ----
		mEnvironmentBusHandle = mSoloud->play(mEnvironmentBus);
		mMusicBusHandle = mSoloud->play(mMusicBus);
		mSfxBusHandle = mSoloud->play(mSfxBus);

		mSoloud->setVolume(mEnvironmentBusHandle, 0.85f);
		mSoloud->setVolume(mMusicBusHandle, mMusicVolume);
		mSoloud->setVolume(mSfxBusHandle, mSfxVolume);

		// ---- SFXR 程序化音效 ----

		// 1. 室外风声 — HURT预设改，低频噪声
		mSfxrWind.loadPreset(SoLoud::Sfxr::HURT, 12345);
		mSfxrWind.mParams.wave_type = 3;
		mSfxrWind.mParams.p_env_attack = 0.3f;
		mSfxrWind.mParams.p_env_sustain = 0.4f;
		mSfxrWind.mParams.p_env_decay = 0.3f;
		mSfxrWind.mParams.p_env_punch = 0.1f;
		mSfxrWind.mParams.p_base_freq = 0.15f * 44100 / 22050;
		mSfxrWind.mParams.p_freq_ramp = -0.1f;
		mSfxrWind.setLooping(true);
		mSfxrWind.setFilter(0, &mOcclusionLPF);

		// 2. 音乐室旋律 — POWERUP预设改，方波
		mSfxrMusic.loadPreset(SoLoud::Sfxr::POWERUP, 42);
		mSfxrMusic.mParams.wave_type = 2;
		mSfxrMusic.mParams.p_base_freq = 0.4f * 44100 / 22050;
		mSfxrMusic.mParams.p_env_attack = 0.1f;
		mSfxrMusic.mParams.p_env_sustain = 0.3f;
		mSfxrMusic.mParams.p_env_decay = 0.3f;
		mSfxrMusic.mParams.p_freq_ramp = -0.2f;
		mSfxrMusic.setLooping(true);
		mSfxrMusic.setFilter(0, &mOcclusionLPF);

		// 3. 水滴声 — COIN预设改，正弦波短促音
		mSfxrWater.loadPreset(SoLoud::Sfxr::COIN, 99);
		mSfxrWater.mParams.wave_type = 1;
		mSfxrWater.mParams.p_base_freq = 0.6f * 44100 / 22050;
		mSfxrWater.mParams.p_env_sustain = 0.1f;
		mSfxrWater.mParams.p_env_decay = 0.2f;
		mSfxrWater.setLooping(true);
		mSfxrWater.setFilter(0, &mOcclusionLPF);

		// 4. 走廊嗡嗡声 — BLIP预设改，极低频
		mSfxrHum.loadPreset(SoLoud::Sfxr::BLIP, 77);
		mSfxrHum.mParams.wave_type = 0;
		mSfxrHum.mParams.p_base_freq = 0.08f * 44100 / 22050;
		mSfxrHum.mParams.p_env_attack = 0.2f;
		mSfxrHum.mParams.p_env_sustain = 0.5f;
		mSfxrHum.mParams.p_env_decay = 0.2f;
		mSfxrHum.setLooping(true);
		mSfxrHum.setFilter(0, &mOcclusionLPF);

		// 5. NEW 工坊机器轰鸣 — LASER预设改，工业感低频方波+颤音
		mSfxrMachine.loadPreset(SoLoud::Sfxr::LASER, 555);
		mSfxrMachine.mParams.wave_type = 0;
		mSfxrMachine.mParams.p_base_freq = 0.14f * 44100 / 22050;
		mSfxrMachine.mParams.p_freq_limit = 0.3f;
		mSfxrMachine.mParams.p_freq_ramp = -0.08f;
		mSfxrMachine.mParams.p_env_attack = 0.4f;
		mSfxrMachine.mParams.p_env_sustain = 0.35f;
		mSfxrMachine.mParams.p_env_decay = 0.3f;
		mSfxrMachine.mParams.p_vib_strength = 0.2f;
		mSfxrMachine.mParams.p_vib_speed = 0.3f;
		mSfxrMachine.mParams.p_arp_mod = -0.15f;
		mSfxrMachine.setLooping(true);
		mSfxrMachine.setFilter(0, &mOcclusionLPF);

		// 6. NEW 室外雨声 — HURT预设改，白噪声+低频滚雷
		mSfxrRain.loadPreset(SoLoud::Sfxr::HURT, 7777);
		mSfxrRain.mParams.wave_type = 3;
		mSfxrRain.mParams.p_base_freq = 0.22f * 44100 / 22050;
		mSfxrRain.mParams.p_freq_limit = 0.6f;
		mSfxrRain.mParams.p_freq_ramp = 0.05f;
		mSfxrRain.mParams.p_env_attack = 0.5f;
		mSfxrRain.mParams.p_env_sustain = 0.45f;
		mSfxrRain.mParams.p_env_decay = 0.3f;
		mSfxrRain.mParams.p_env_punch = 0.05f;
		mSfxrRain.mParams.p_arp_mod = 0.1f;
		mSfxrRain.setLooping(true);
		mSfxrRain.setFilter(0, &mOcclusionLPF);

		// 7. NEW 实验室电子音 — BLIP预设改，高频短促正弦波循环
		mSfxrLab.loadPreset(SoLoud::Sfxr::BLIP, 3333);
		mSfxrLab.mParams.wave_type = 1;
		mSfxrLab.mParams.p_base_freq = 0.55f * 44100 / 22050;
		mSfxrLab.mParams.p_freq_limit = 0.25f;
		mSfxrLab.mParams.p_freq_ramp = 0.15f;
		mSfxrLab.mParams.p_env_attack = 0.02f;
		mSfxrLab.mParams.p_env_sustain = 0.08f;
		mSfxrLab.mParams.p_env_decay = 0.15f;
		mSfxrLab.mParams.p_arp_mod = 0.3f;
		mSfxrLab.mParams.p_arp_speed = 0.6f;
		mSfxrLab.setLooping(true);
		mSfxrLab.setFilter(0, &mOcclusionLPF);

		// 脚步声 — HURT预设，极短促噪声
		mSfxrFootstep.loadPreset(SoLoud::Sfxr::HURT, 55);
		mSfxrFootstep.mParams.wave_type = 3;
		mSfxrFootstep.mParams.p_env_attack = 0.0f;
		mSfxrFootstep.mParams.p_env_sustain = 0.01f;
		mSfxrFootstep.mParams.p_env_decay = 0.05f;
		mSfxrFootstep.mParams.p_env_punch = 0.4f;
		mSfxrFootstep.mParams.p_base_freq = 0.3f * 44100 / 22050;

		// 碰撞冲击声 — EXPLOSION预设
		mSfxrImpact.loadPreset(SoLoud::Sfxr::EXPLOSION, 33);
		mSfxrImpact.mParams.p_base_freq = 0.2f * 44100 / 22050;
		mSfxrImpact.mParams.p_env_decay = 0.3f;

		// 区域过渡提示音 — BLIP预设
		mSfxrBlip.loadPreset(SoLoud::Sfxr::BLIP, 88);

		// ---- 创建声源并播放 ----
		Vec2_ centrality = mMapStatic->centrality;
		mSoundSources.clear();

		// 声源1：室外风声 (地图左上角单体柱附近)
		{
			SoundSource src;
			src.name = u8"室外风声";
			src.position = Vec2_{3, 35} - centrality;
			src.sfxr = &mSfxrWind;
			src.baseVolume = 0.6f;
			src.maxDistance = 35.0f;
			src.active = false;
			src.color = {0.5f, 0.75f, 1.0f, 1.0f};
			src.wallCount = 0;
			src.handle = mEnvironmentBus.play(mSfxrWind, 0.0f, 0.0f);
			mSoloud->setPause(src.handle, false);
			mSoundSources.push_back(src);
		}

		// 声源2：音乐室旋律 (A1室内)
		{
			SoundSource src;
			src.name = u8"音乐室旋律";
			src.position = Vec2_{11, 21} - centrality;
			src.sfxr = &mSfxrMusic;
			src.baseVolume = 0.7f;
			src.maxDistance = 28.0f;
			src.active = false;
			src.color = {1.0f, 0.85f, 0.3f, 1.0f};
			src.wallCount = 0;
			src.handle = mMusicBus.play(mSfxrMusic, 0.0f, 0.0f);
			mSoloud->setPause(src.handle, false);
			mSoundSources.push_back(src);
		}

		// 声源3：水滴声 (B1水泵房)
		{
			SoundSource src;
			src.name = u8"水滴声";
			src.position = Vec2_{40, 22} - centrality;
			src.sfxr = &mSfxrWater;
			src.baseVolume = 0.55f;
			src.maxDistance = 28.0f;
			src.active = false;
			src.color = {0.3f, 0.6f, 1.0f, 1.0f};
			src.wallCount = 0;
			src.handle = mEnvironmentBus.play(mSfxrWater, 0.0f, 0.0f);
			mSoloud->setPause(src.handle, false);
			mSoundSources.push_back(src);
		}

		// 声源4：走廊嗡嗡声 (A楼下层走廊中央)
		{
			SoundSource src;
			src.name = u8"走廊嗡嗡声";
			src.position = Vec2_{18, 6} - centrality;
			src.sfxr = &mSfxrHum;
			src.baseVolume = 0.5f;
			src.maxDistance = 20.0f;
			src.active = false;
			src.color = {1.0f, 0.5f, 0.2f, 1.0f};
			src.wallCount = 0;
			src.handle = mEnvironmentBus.play(mSfxrHum, 0.0f, 0.0f);
			mSoloud->setPause(src.handle, false);
			mSoundSources.push_back(src);
		}

		// 声源5：NEW 工坊机器轰鸣声 (A4工坊内)
		{
			SoundSource src;
			src.name = u8"工坊机器";
			src.position = Vec2_{25, 12} - centrality;
			src.sfxr = &mSfxrMachine;
			src.baseVolume = 0.55f;
			src.maxDistance = 24.0f;
			src.active = false;
			src.color = {1.0f, 0.6f, 0.2f, 1.0f};
			src.wallCount = 0;
			src.handle = mEnvironmentBus.play(mSfxrMachine, 0.0f, 0.0f);
			mSoloud->setPause(src.handle, false);
			mSoundSources.push_back(src);
		}

		// 声源6：NEW 室外雨声 (地图右上角)
		{
			SoundSource src;
			src.name = u8"室外雨声";
			src.position = Vec2_{56, 36} - centrality;
			src.sfxr = &mSfxrRain;
			src.baseVolume = 0.6f;
			src.maxDistance = 38.0f;
			src.active = false;
			src.color = {0.4f, 0.5f, 0.8f, 1.0f};
			src.wallCount = 0;
			src.handle = mEnvironmentBus.play(mSfxrRain, 0.0f, 0.0f);
			mSoloud->setPause(src.handle, false);
			mSoundSources.push_back(src);
		}

		// 声源7：NEW 实验室电子音 (B2实验室内)
		{
			SoundSource src;
			src.name = u8"实验室电音";
			src.position = Vec2_{52, 21} - centrality;
			src.sfxr = &mSfxrLab;
			src.baseVolume = 0.5f;
			src.maxDistance = 22.0f;
			src.active = false;
			src.color = {0.8f, 0.4f, 1.0f, 1.0f};
			src.wallCount = 0;
			src.handle = mSfxBus.play(mSfxrLab, 0.0f, 0.0f);
			mSoloud->setPause(src.handle, false);
			mSoundSources.push_back(src);
		}
	}

	// ==================== 环境检测 ====================

	bool SoloudTest::IsPositionIndoor(Vec2_ pos)
	{
		Vec2_ mapPos = pos + mMapStatic->centrality;
		int x = (int)floor(mapPos.x);
		int y = (int)floor(mapPos.y);

		for (int checkY = y + 1; checkY < MAP_HEIGHT; ++checkY) {
			if (x >= 0 && x < MAP_WIDTH && checkY >= 0 && checkY < MAP_HEIGHT) {
				if (mMapStatic->at(x, checkY).Entity) {
					return true;
				}
			}
		}
		return false;
	}

	int SoloudTest::CountWallsBetween(Vec2_ from, Vec2_ to)
	{
		Vec2_ mapFrom = from + mMapStatic->centrality;
		Vec2_ mapTo = to + mMapStatic->centrality;

		int wallCount = 0;
		int steps = (int)(PhysicsBlock::Modulus(mapTo - mapFrom) * 2) + 1;
		if (steps < 2) steps = 2;
		if (steps > 200) steps = 200;

		bool lastWasWall = false;
		for (int i = 0; i <= steps; ++i) {
			float t = (float)i / (float)steps;
			Vec2_ checkPos = mapFrom + (mapTo - mapFrom) * t;
			int cx = (int)floor(checkPos.x);
			int cy = (int)floor(checkPos.y);

			if (cx >= 0 && cx < MAP_WIDTH && cy >= 0 && cy < MAP_HEIGHT) {
				bool isWall = mMapStatic->at(cx, cy).Collision;
				if (isWall && !lastWasWall) {
					wallCount++;
				}
				lastWasWall = isWall;
			}
		}
		return wallCount;
	}

	bool SoloudTest::IsPlayerOnGround()
	{
		Vec2_ below = mPlayer->pos + Vec2_{0, -(mPlayer->radius + 0.2)};
		Vec2_ mapPos = below + mMapStatic->centrality;
		int x = (int)floor(mapPos.x);
		int y = (int)floor(mapPos.y);
		if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
			return mMapStatic->at(x, y).Collision;
		}
		return false;
	}

	// ==================== 音效播放 ====================

	void SoloudTest::PlayFootstep()
	{
		float vol = mSfxVolume * 0.65f;
		if (mIndoorBlend > 0.5f) {
			vol *= 0.7f;
		}
		mSfxBus.play(mSfxrFootstep, vol, 0);
	}

	void SoloudTest::PlayImpactSound(float intensity)
	{
		float vol = glm::clamp(intensity / 40.0f, 0.15f, 1.0f) * mSfxVolume;
		mSfxBus.play(mSfxrImpact, vol, 0);
	}

	void SoloudTest::PlayZoneTransitionSound(bool enteringIndoor)
	{
		float vol = mSfxVolume * 0.35f;
		SoLoud::handle h = mSfxBus.play(mSfxrBlip, vol, 0);
		if (enteringIndoor) {
			mSoloud->setRelativePlaySpeed(h, 0.75f);
		}
		else {
			mSoloud->setRelativePlaySpeed(h, 1.35f);
		}
	}

	// ==================== 音频状态更新 ====================

	void SoloudTest::UpdateAudio()
	{
		if (!mPlayer || !mSoloud) return;

		Vec2_ playerPos = mPlayer->pos;
		bool isIndoor = IsPositionIndoor(playerPos);

		if (isIndoor != mLastIndoorState) {
			PlayZoneTransitionSound(isIndoor);
			mLastIndoorState = isIndoor;
		}

		float targetIndoor = isIndoor ? 1.0f : 0.0f;
		float blendFactor = (float)glm::min((double)(TOOL::FPStime * 5.0f), 1.0);
		mIndoorBlend += (targetIndoor - mIndoorBlend) * blendFactor;

		// 走廊检测：A楼下层走廊 + B楼大厅
		Vec2_ mapPos = playerPos + mMapStatic->centrality;
		bool inCorridor = false;
		if (mapPos.x >= 6 && mapPos.x <= 29 && mapPos.y >= 5 && mapPos.y <= 7) inCorridor = true;
		if (mapPos.x >= 35 && mapPos.x <= 57 && mapPos.y >= 5 && mapPos.y <= 11) inCorridor = true;

		float targetCorridor = inCorridor ? 1.0f : 0.0f;
		blendFactor = (float)glm::min((double)(TOOL::FPStime * 5.0f), 1.0);
		mCorridorBlend += (targetCorridor - mCorridorBlend) * blendFactor;

		mReverbWet = mIndoorBlend * 0.7f;
		mReverbRoomSize = mIndoorBlend * 0.8f;
		mEchoWet = mCorridorBlend * 0.5f;
		mLofiWet = mCorridorBlend * 0.35f;

		mSoloud->fadeFilterParameter(mEnvironmentBusHandle, 0,
			SoLoud::FreeverbFilter::WET, mReverbWet, 0.1f);
		mSoloud->fadeFilterParameter(mEnvironmentBusHandle, 0,
			SoLoud::FreeverbFilter::ROOMSIZE, mReverbRoomSize, 0.1f);
		mSoloud->fadeFilterParameter(mEnvironmentBusHandle, 1,
			SoLoud::EchoFilter::WET, mEchoWet, 0.1f);
		mSoloud->fadeFilterParameter(mSfxBusHandle, 0,
			SoLoud::LofiFilter::WET, mLofiWet, 0.1f);

		// 遍历所有声源
		for (auto &src : mSoundSources) {
			float distance = (float)PhysicsBlock::Modulus(playerPos - src.position);
			float attenuation = 1.0f / (1.0f + distance * distance * 0.015f);
			attenuation = glm::clamp(attenuation, 0.0f, 1.0f);

			if (distance > src.maxDistance) {
				attenuation = 0.0f;
			}

			int walls = CountWallsBetween(playerPos, src.position);
			src.wallCount = walls;

			float occlusionFactor = 1.0f;
			float lpfFreq = 22000.0f;
			if (walls > 0) {
				occlusionFactor = 1.0f / (1.0f + walls * 0.8f);
				lpfFreq = glm::max(400.0f, 22000.0f / (1.0f + walls * 3.0f));
			}

			float volume = src.baseVolume * attenuation * occlusionFactor * mMasterVolume;
			mSoloud->setVolume(src.handle, volume);

			float pan = (float)((src.position.x - playerPos.x) / src.maxDistance);
			pan = glm::clamp(pan, -1.0f, 1.0f);
			mSoloud->setPan(src.handle, pan);

			mSoloud->fadeFilterParameter(src.handle, 0,
				SoLoud::BiquadResonantFilter::FREQUENCY, lpfFreq, 0.15f);
		}

		mSoloud->setVolume(mEnvironmentBusHandle, 0.85f * mMasterVolume);
		mSoloud->setVolume(mMusicBusHandle, mMusicVolume * mMasterVolume);
		mSoloud->setVolume(mSfxBusHandle, mSfxVolume * mMasterVolume);

		// 脚步声
		float speed = (float)PhysicsBlock::Modulus(mPlayer->speed);
		if (speed > 1.0f && IsPlayerOnGround()) {
			mFootstepTimer -= TOOL::FPStime;
			if (mFootstepTimer <= 0) {
				PlayFootstep();
				mFootstepTimer = glm::max(0.2f, 0.5f - speed * 0.02f);
			}
		}
		else {
			mFootstepTimer = 0;
		}

		// 碰撞冲击检测
		Vec2_ speedChange = mPlayer->speed - mPlayerPrevSpeed;
		float impactIntensity = (float)PhysicsBlock::Modulus(speedChange);
		if (impactIntensity > 8.0f) {
			PlayImpactSound(impactIntensity);
		}
		mPlayerPrevSpeed = mPlayer->speed;
	}

	// ==================== 输入事件 ====================

	void SoloudTest::MouseMove(double xpos, double ypos)
	{
	}

	void SoloudTest::MouseRoller(int z)
	{
		if (Global::ClickWindow) return;

		glm::vec3 camPos = mCamera->getCameraPos();
		if (camPos.z <= 10) {
			camPos.z += (camPos.z / 2) * z;
		}
		else {
			camPos.z += z * 5;
		}
		if (camPos.z <= 0.1f) camPos.z = 0.1f;
		if (camPos.z > 200.0f) camPos.z = 200.0f;
		mCameraTarget.z = camPos.z;
	}

	void SoloudTest::KeyDown(GameKeyEnum moveDirection)
	{
		if (!mPlayer) return;

		switch (moveDirection)
		{
		case GameKeyEnum::MOVE_LEFT:
			mPlayer->AddForce({-PLAYER_FORCE, 0});
			break;
		case GameKeyEnum::MOVE_RIGHT:
			mPlayer->AddForce({PLAYER_FORCE, 0});
			break;
		case GameKeyEnum::MOVE_FRONT:
			mPlayer->AddForce({0, PLAYER_JUMP_FORCE});
			break;
		case GameKeyEnum::MOVE_BACK:
			mPlayer->AddForce({0, -PLAYER_FORCE});
			break;
		case GameKeyEnum::ESC:
			if (Global::ConsoleBool) {
				Global::ConsoleBool = false;
				InterFace->ConsoleFocusHere = true;
			}
			else {
				InterFace->SetInterFaceBool();
			}
			break;
		case GameKeyEnum::Key_1:
			mDebugVisualization = !mDebugVisualization;
			break;
		case GameKeyEnum::Key_2:
			mPhysicsSwitch = !mPhysicsSwitch;
			break;
		default:
			break;
		}
	}

	// ==================== 游戏主循环 ====================

	void SoloudTest::GameCommandBuffers(unsigned int Format_i)
	{
		mAuxiliaryVision->GetCommandBuffer(wThreadCommandBufferS, Format_i);
	}

	void SoloudTest::GameLoop(unsigned int mCurrentFrame)
	{
		mAuxiliaryVision->Begin();

		if (mPhysicsSwitch) {
			#define PhysicsTick (1.0 / 100.0)
			static float AddUpTime = 0;
			AddUpTime += TOOL::FPStime;
			if (AddUpTime > PhysicsTick) {
				AddUpTime -= PhysicsTick;
				mPhysicsWorld->PhysicsEmulator(PhysicsTick);
				if (AddUpTime > 1.0) AddUpTime = 0.1;
			}
		}
		else {
			mPhysicsWorld->PhysicsInformationUpdate();
		}

		UpdateAudio();
		RenderScene();

		mAuxiliaryVision->End();

		if (mPlayer) {
			glm::vec3 currentPos = mCamera->getCameraPos();
			glm::vec3 targetPos = {(float)mPlayer->pos.x, (float)mPlayer->pos.y, mCameraTarget.z};
			float cameraBlendFactor = (float)glm::min((double)(TOOL::FPStime * 5.0f), 1.0);
			glm::vec3 newPos = glm::mix(currentPos, targetPos, cameraBlendFactor);
			mCamera->setCameraPos(newPos);
		}

		mCamera->update();
		VPMatrices *mVPMatrices = (VPMatrices *)mCameraVPMatricesBuffer[mCurrentFrame]->getupdateBufferByMap();
		mVPMatrices->mViewMatrix = mCamera->getViewMatrix();
		mCameraVPMatricesBuffer[mCurrentFrame]->endupdateBufferByMap();
	}

	// ==================== 渲染 ====================

	void SoloudTest::RenderScene()
	{
		if (!mPhysicsWorld) return;

		// 网格形状
		for (auto i : mPhysicsWorld->GetPhysicsShape()) {
			for (size_t x = 0; x < i->width; ++x) {
				for (size_t y = 0; y < i->height; ++y) {
					if (i->at(x, y).Entity) {
						ShowSquare(
							PhysicsBlock::vec2angle(Vec2_{(FLOAT_)x, (FLOAT_)y} - i->CentreMass, i->angle) + i->pos,
							i->angle,
							{0.5f, (i->StaticNum < 10 ? 0.7f : 0.2f), 0.15f, 1});
					}
				}
			}
		}

		// 粒子
		for (auto i : mPhysicsWorld->GetPhysicsParticle()) {
			mAuxiliaryVision->Spot({i->pos, 0}, 0.05f, {0.5f, (i->StaticNum < 10 ? 0.7f : 0.2f), 0.15f, 1});
		}

		// 圆形
		for (auto i : mPhysicsWorld->GetPhysicsCircle()) {
			glm::vec4 color;
			if (i == mPlayer) {
				color = {0.3f, 0.5f, 1.0f, 1.0f};
			}
			else {
				color = {0.5f, (i->StaticNum < 10 ? 0.75f : 0.2f), 0.2f, 1};
			}
			mAuxiliaryVision->Circle({i->pos, 0}, i->radius, color);

			if (i == mPlayer) {
				mAuxiliaryVision->Line({i->pos, 0}, {1, 1, 0, 1},
					{i->pos + PhysicsBlock::vec2angle({i->radius, 0}, i->angle), 0}, {1, 1, 0, 1});
			}
		}

		// 线段
		for (auto i : mPhysicsWorld->GetPhysicsLine()) {
			Vec2_ pR = PhysicsBlock::vec2angle({i->radius, 0}, i->angle);
			mAuxiliaryVision->Line({i->pos + pR, 0}, {0, 1, 0, 1}, {i->pos - pR, 0}, {0, 1, 0, 1});
		}

		// 关节
		for (auto i : mPhysicsWorld->GetPhysicsJoint()) {
			mAuxiliaryVision->Line({i->body1->pos, 0}, {0, 1, 0, 1}, {i->body1->pos + i->r1, 0}, {0, 1, 0, 1});
			mAuxiliaryVision->Line({i->body2->pos, 0}, {0, 1, 0, 1}, {i->body2->pos + i->r2, 0}, {0, 1, 0, 1});
		}
		for (auto j : mPhysicsWorld->GetBaseJunction()) {
			mAuxiliaryVision->Line({j->GetA(), 0}, {0, 1, 0, 1}, {j->GetB(), 0}, {0, 1, 0, 1});
		}

		RenderSoundSources();
		if (mDebugVisualization) {
			RenderAudioDebug();
		}
	}

	void SoloudTest::RenderSoundSources()
	{
		for (auto &src : mSoundSources) {
			mAuxiliaryVision->Spot({src.position, 0}, 0.15f, src.color);

			float pulsePhase = fmod((float)glfwGetTime() * 2.0f, 1.0f);
			glm::vec4 pulseColor = src.color * (0.5f + pulsePhase * 0.5f);
			pulseColor.a = 0.5f;
			mAuxiliaryVision->Circle({src.position, 0}, 0.3f, pulseColor);

			if (mShowSoundRadius) {
				glm::vec4 radiusColor = src.color;
				radiusColor.a = 0.15f;
				mAuxiliaryVision->Circle({src.position, 0}, src.maxDistance, radiusColor);
			}
		}
	}

	void SoloudTest::RenderAudioDebug()
	{
		if (!mPlayer) return;

		Vec2_ playerPos = mPlayer->pos;

		for (auto &src : mSoundSources) {
			if (mShowOcclusionRays) {
				glm::vec4 rayColor = src.wallCount > 0 ? glm::vec4{1, 0.2, 0.2, 0.5} : glm::vec4{0.2, 1, 0.2, 0.5};
				mAuxiliaryVision->Line({playerPos, 0}, rayColor, {src.position, 0}, rayColor);
			}
		}

		if (mIndoorBlend > 0.1f) {
			glm::vec4 indoorColor = {0.2, 0.5, 1.0, mIndoorBlend * 0.3f};
			mAuxiliaryVision->Circle({playerPos, 0}, 1.5f, indoorColor);
		}
	}

	// ==================== ImGui UI (全中文) ====================

	void SoloudTest::GameUI()
	{
		ImGui::Begin(u8"🔊 SoLoud 环境音效演示");

		ImVec2 window_pos = ImGui::GetWindowPos();
		ImVec2 window_size = ImGui::GetWindowSize();
		if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
			((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY))) {
			Global::ClickWindow = true;
		}

		// ---- 音量控制 ----
		ImGui::TextColored({0.3f, 0.9f, 0.5f, 1.0f}, u8"── 音量控制 ──");
		ImGui::SliderFloat(u8"主音量", &mMasterVolume, 0.0f, 1.5f);
		ImGui::SliderFloat(u8"音乐音量", &mMusicVolume, 0.0f, 1.0f);
		ImGui::SliderFloat(u8"音效音量", &mSfxVolume, 0.0f, 1.0f);

		// ---- 环境状态 ----
		ImGui::Separator();
		ImGui::TextColored({0.3f, 0.7f, 1.0f, 1.0f}, u8"── 环境状态 ──");

		const char *zoneLabel;
		ImVec4 zoneColor;
		if (mCorridorBlend > 0.3f) {
			zoneLabel = u8"走廊区域";
			zoneColor = {1.0f, 0.6f, 0.2f, 1.0f};
		}
		else if (mIndoorBlend > 0.5f) {
			zoneLabel = u8"室内";
			zoneColor = {0.3f, 0.7f, 1.0f, 1.0f};
		}
		else {
			zoneLabel = u8"室外";
			zoneColor = {0.5f, 1.0f, 0.5f, 1.0f};
		}
		ImGui::TextColored(zoneColor, u8"当前区域: %s", zoneLabel);
		ImGui::Text(u8"室内系数: %.2f", mIndoorBlend);
		ImGui::Text(u8"走廊系数: %.2f", mCorridorBlend);

		// ---- 音频效果 ----
		ImGui::Separator();
		ImGui::TextColored({1.0f, 0.8f, 0.4f, 1.0f}, u8"── 音频效果 ──");
		ImGui::Text(u8"混响湿信号:  %.2f", mReverbWet);
		ImGui::ProgressBar(mReverbWet / 0.7f, {-1, 12}, "");
		ImGui::Text(u8"混响房间大小: %.2f", mReverbRoomSize);
		ImGui::ProgressBar(mReverbRoomSize / 0.8f, {-1, 12}, "");
		ImGui::Text(u8"回声湿信号:  %.2f", mEchoWet);
		ImGui::ProgressBar(mEchoWet / 0.5f, {-1, 12}, "");
		ImGui::Text(u8"LoFi失真:   %.2f", mLofiWet);
		ImGui::ProgressBar(mLofiWet / 0.35f, {-1, 12}, "");

		// ---- 声源列表 ----
		ImGui::Separator();
		ImGui::TextColored({0.9f, 0.6f, 0.3f, 1.0f}, u8"── 声源列表 (%d个) ──", (int)mSoundSources.size());
		ImGui::BeginChild("Sources", {0, 140}, true);
		for (auto &src : mSoundSources) {
			float dist = mPlayer ? (float)PhysicsBlock::Modulus(mPlayer->pos - src.position) : 0;
			float vol = 0;
			if (mSoloud->isValidVoiceHandle(src.handle)) {
				vol = mSoloud->getVolume(src.handle);
			}
			ImGui::PushStyleColor(ImGuiCol_Text, {src.color.r, src.color.g, src.color.b, src.color.a});
			ImGui::Text(u8"◆ %s", src.name.c_str());
			ImGui::PopStyleColor();
			ImGui::SameLine(200);
			ImGui::TextColored(vol > 0.05f ? ImVec4{0.4f, 1, 0.4f, 1} : ImVec4{0.5f, 0.5f, 0.5f, 1},
				u8"距:%.1f 墙:%d 音:%.2f", dist, src.wallCount, vol);
		}
		ImGui::EndChild();

		// ---- 调试开关 ----
		ImGui::Separator();
		ImGui::TextColored({0.7f, 0.7f, 0.7f, 1.0f}, u8"── 调试开关 ──");
		ImGui::Checkbox(u8"音频调试可视化", &mDebugVisualization);
		ImGui::SameLine();
		ImGui::Checkbox(u8"显示声音半径", &mShowSoundRadius);
		ImGui::Checkbox(u8"显示遮挡射线", &mShowOcclusionRays);
		if (ImGui::Button(mPhysicsSwitch ? u8"■ 暂停物理" : u8"▶ 恢复物理", {100, 22})) {
			mPhysicsSwitch = !mPhysicsSwitch;
		}
		ImGui::SameLine();
		ImGui::TextColored(mPhysicsSwitch ? ImVec4{0.4f, 1, 0.4f, 1} : ImVec4{1, 0.4f, 0.4f, 1},
			mPhysicsSwitch ? u8"运行中" : u8"已暂停");

		// ---- 操作提示 ----
		ImGui::Separator();
		ImGui::TextColored({0.6f, 0.6f, 1.0f, 1.0f}, u8"── 操作说明 ──");
		ImGui::BulletText(u8"W/A/S/D 或 方向键 — 上下左右自由移动（上帝视角）");
		ImGui::BulletText(u8"1 — 切换调试可视化    2 — 暂停/恢复物理");
		ImGui::BulletText(u8"滚轮 — 缩放相机");

		// ---- 玩家状态 ----
		if (mPlayer) {
			ImGui::Separator();
			ImGui::TextColored({0.3f, 0.5f, 1.0f, 1.0f}, u8"── 玩家状态 ──");
			ImGui::Text(u8"位置: (%.1f, %.1f)  速度: (%.1f, %.1f)",
				mPlayer->pos.x, mPlayer->pos.y, mPlayer->speed.x, mPlayer->speed.y);
			ImGui::Text(u8"着地: %s", IsPlayerOnGround() ? u8"是" : u8"否");
			float spd = (float)PhysicsBlock::Modulus(mPlayer->speed);
			ImGui::Text(u8"速度: %.1f", spd);
		}

		ImGui::End();
	}

	// ==================== 其他接口 ====================

	void SoloudTest::GameRecordCommandBuffers()
	{
		mAuxiliaryVision->initCommandBuffer();
	}

	void SoloudTest::GameStopInterfaceLoop(unsigned int mCurrentFrame)
	{
	}

	void SoloudTest::GameTCPLoop()
	{
		if (Global::MultiplePeopleMode) {
			if (Global::ServerOrClient) {
				event_base_loop(server::GetServer()->GetEvent_Base(), EVLOOP_NONBLOCK);
			}
			else {
				event_base_loop(client::GetClient()->GetEvent_Base(), EVLOOP_ONCE);
			}
		}
	}

	// ==================== 渲染辅助 ====================

	void SoloudTest::ShowStaticSquare(glm::dvec2 pos, double angle, glm::vec4 color)
	{
		glm::dvec2 Angle = PhysicsBlock::AngleFloatToAngleVec(angle);
		glm::dvec2 jiao1 = PhysicsBlock::vec2angle({0, 1}, Angle);
		glm::dvec2 jiao2 = PhysicsBlock::vec2angle({1, 0}, Angle);
		glm::dvec2 jiao3 = jiao2 + jiao1;
		mAuxiliaryVision->AddStaticLine({pos, 0}, {pos + jiao1, 0}, color);
		mAuxiliaryVision->AddStaticLine({pos, 0}, {pos + jiao2, 0}, color);
		mAuxiliaryVision->AddStaticLine({pos + jiao3, 0}, {pos + jiao1, 0}, color);
		mAuxiliaryVision->AddStaticLine({pos + jiao3, 0}, {pos + jiao2, 0}, color);
	}

	void SoloudTest::ShowSquare(glm::dvec2 pos, double angle, glm::vec4 color)
	{
		glm::dvec2 Angle = PhysicsBlock::AngleFloatToAngleVec(angle);
		glm::dvec2 jiao1 = PhysicsBlock::vec2angle({0, 1}, Angle);
		glm::dvec2 jiao2 = PhysicsBlock::vec2angle({1, 0}, Angle);
		glm::dvec2 jiao3 = jiao2 + jiao1;
		mAuxiliaryVision->Line({pos, 0}, color, {pos + jiao1, 0}, color);
		mAuxiliaryVision->Line({pos, 0}, color, {pos + jiao2, 0}, color);
		mAuxiliaryVision->Line({pos + jiao3, 0}, color, {pos + jiao1, 0}, color);
		mAuxiliaryVision->Line({pos + jiao3, 0}, color, {pos + jiao2, 0}, color);
	}

}
