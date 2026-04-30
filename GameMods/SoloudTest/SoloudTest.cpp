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
		// 初始化辅助线渲染器，最大 100000 个顶点
		mAuxiliaryVision = new VulKan::AuxiliaryVision(mDevice, mPipelineS, 100000);
		mAuxiliaryVision->initUniformManager(
			mSwapChain->getImageCount(),
			mCameraVPMatricesBuffer);
		mAuxiliaryVision->RecordingCommandBuffer(mRenderPass, mSwapChain);

		// 从全局 SoundEffect 单例获取 SoLoud 引擎指针
		mSoloud = SoundEffect::SoundEffect::GetSoundEffect()->GetSoloud();
		// 停止其他模组可能正在播放的音频，避免冲突
		SoundEffect::SoundEffect::GetSoundEffect()->StopAll();

		CreateScene();
		InitAudio();

		mCamera->setCameraPos({0, 0, 30});
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
	// 创建 40×25 的侧视图地图，包含：
	//   - 四周边界墙
	//   - 一个大型建筑（左墙 x=14, 右墙 x=32, 地板 y=6, 天花板 y=18）
	//   - 左墙门洞 (x=14, y=11~12)
	//   - 内部分隔墙 (x=23)，将建筑分为 Room A (左) 和 Room B (右)
	//   - 分隔墙门洞 (x=23, y=10~12)
	//   - 走廊天花板 (y=7)，将建筑下部隔为走廊
	//   - 走廊天花板门洞 (x=18~19)
	//   - 玩家圆形、装饰球、可推动方块

	void SoloudTest::CreateScene()
	{
		// 创建物理世界，重力向下 9.8，不开启风力
		mPhysicsWorld = new PhysicsBlock::PhysicsWorld({0.0, -9.8}, false);

		// 创建空白地图
		mMapStatic = new PhysicsBlock::MapStatic(MAP_WIDTH, MAP_HEIGHT);

		// 先将所有格子清空
		for (int i = 0; i < MAP_WIDTH * MAP_HEIGHT; ++i) {
			mMapStatic->at(i).Entity = false;
			mMapStatic->at(i).Collision = false;
			mMapStatic->at(i).mass = 1.0;
		}

		// 四周边界墙
		for (int i = 0; i < MAP_WIDTH; ++i) {
			mMapStatic->at(i, 0).Entity = true;       // 底部
			mMapStatic->at(i, 0).Collision = true;
			mMapStatic->at(i, MAP_HEIGHT - 1).Entity = true;  // 顶部
			mMapStatic->at(i, MAP_HEIGHT - 1).Collision = true;
		}
		for (int i = 0; i < MAP_HEIGHT; ++i) {
			mMapStatic->at(0, i).Entity = true;       // 左侧
			mMapStatic->at(0, i).Collision = true;
			mMapStatic->at(MAP_WIDTH - 1, i).Entity = true;  // 右侧
			mMapStatic->at(MAP_WIDTH - 1, i).Collision = true;
		}

		// 建筑四面墙：左墙 x=14, 右墙 x=32, 地板 y=6, 天花板 y=18
		int bx1 = 14, bx2 = 32;
		int by1 = 6, by2 = 18;

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

		// 左墙门洞 (x=14, y=11~12) — 从室外进入建筑的入口
		mMapStatic->at(bx1, 11).Entity = false;
		mMapStatic->at(bx1, 11).Collision = false;
		mMapStatic->at(bx1, 12).Entity = false;
		mMapStatic->at(bx1, 12).Collision = false;

		// 内部分隔墙 (x=23)，将建筑分为 Room A (左) 和 Room B (右)
		int wx = 23;
		for (int y = by1; y <= by2; ++y) {
			if (y != 10 && y != 11 && y != 12) {  // 分隔墙门洞 (y=10~12)
				mMapStatic->at(wx, y).Entity = true;
				mMapStatic->at(wx, y).Collision = true;
			}
		}

		// 走廊天花板 (y=7)，将建筑下部隔为走廊区域
		int cx1 = bx1 + 1, cx2 = bx2 - 1;
		int cy1 = 7, cy2 = 9;
		for (int x = cx1; x <= cx2; ++x) {
			mMapStatic->at(x, cy1).Entity = true;
			mMapStatic->at(x, cy1).Collision = true;
		}
		// 走廊天花板门洞 (x=18~19) — 从走廊进入上层房间的楼梯口
		mMapStatic->at(18, cy1).Entity = false;
		mMapStatic->at(18, cy1).Collision = false;
		mMapStatic->at(19, cy1).Entity = false;
		mMapStatic->at(19, cy1).Collision = false;

		// 设置地图中心点，将地图坐标转换为以中心为原点的世界坐标
		mMapStatic->SetCentrality({MAP_WIDTH / 2.0, MAP_HEIGHT / 2.0});
		mPhysicsWorld->SetMapFormwork(mMapStatic);

		// 渲染所有静态地图方块
		for (size_t x = 0; x < mMapStatic->width; x++) {
			for (size_t y = 0; y < mMapStatic->height; y++) {
				if (mMapStatic->at(x, y).Entity) {
					Vec2_ worldPos = Vec2_{(FLOAT_)x, (FLOAT_)y} - mMapStatic->centrality;
					// 内墙和走廊天花板用不同颜色标识
					bool isInteriorWall = (x == wx && y >= by1 && y <= by2 && y != 10 && y != 11 && y != 12);
					bool isCorridorCeiling = (y == cy1 && x >= cx1 && x <= cx2 && x != 18 && x != 19);
					glm::vec4 color = {0, 1, 0, 1};
					if (isInteriorWall || isCorridorCeiling) {
						color = {0, 0.7f, 0.3f, 1};
					}
					ShowStaticSquare(worldPos, 0, color);
				}
			}
		}

		// 创建玩家 — 蓝色圆形，半径 0.5，质量 2.0
		mPlayer = new PhysicsBlock::PhysicsCircle({-8, -8}, 0.5, 2.0, 0.3);
		mPhysicsWorld->AddObject(mPlayer);

		// 装饰球 1
		PhysicsBlock::PhysicsCircle *ball1 = new PhysicsBlock::PhysicsCircle({-3, -6}, 0.6, 1.5, 0.3);
		mPhysicsWorld->AddObject(ball1);

		// 装饰球 2
		PhysicsBlock::PhysicsCircle *ball2 = new PhysicsBlock::PhysicsCircle({5, -5}, 0.4, 1.0, 0.3);
		mPhysicsWorld->AddObject(ball2);

		// 可推动方块 — 3×2 网格形状
		PhysicsBlock::PhysicsShape *box1 = new PhysicsBlock::PhysicsShape({8, -7}, {3, 2});
		for (size_t i = 0; i < box1->width * box1->height; ++i) {
			box1->at(i).Collision = true;
			box1->at(i).Entity = true;
			box1->at(i).mass = 1;
			box1->at(i).FrictionFactor = 0.2;
		}
		box1->UpdateAll();
		box1->angle = 0;
		mPhysicsWorld->AddObject(box1);
	}

	// ==================== 音频初始化 ====================
	//
	// 音频架构：
	//   SoLoud 引擎
	//   ├── Environment Bus（环境声总线）
	//   │   ├── Filter 0: FreeverbFilter（混响 — 室内时增大）
	//   │   ├── Filter 1: EchoFilter（回声 — 走廊时增大）
	//   │   ├── SfxrWind（室外风声，循环）
	//   │   ├── SfxrWater（Room B 水滴声，循环）
	//   │   └── SfxrHum（走廊嗡嗡声，循环）
	//   ├── Music Bus（音乐总线）
	//   │   ├── Filter 0: BiquadResonantFilter LPF（音乐遮挡低通）
	//   │   └── SfxrMusic（Room A 音乐，循环）
	//   └── SFX Bus（音效总线）
	//       ├── Filter 0: LofiFilter（走廊 LoFi 效果）
	//       ├── SfxrFootstep（脚步声，一次性）
	//       ├── SfxrImpact（碰撞冲击声，一次性）
	//       └── SfxrBlip（区域过渡提示音，一次性）
	//
	// 每个循环声源还单独挂载了 BiquadResonantFilter (LOWPASS)，
	// 用于根据墙壁遮挡数量动态调整截止频率。

	void SoloudTest::InitAudio()
	{
		// ---- 初始化滤波器参数 ----
		mReverbFilter.setParams(0, 0.5f, 0.5f, 1.0f);                              // 混响：默认低湿信号
		mOcclusionLPF.setParams(SoLoud::BiquadResonantFilter::LOWPASS, 22000, 1);   // 遮挡 LPF：默认全频通过
		mMusicLPF.setParams(SoLoud::BiquadResonantFilter::LOWPASS, 22000, 1);       // 音乐 LPF：默认全频通过
		mEchoFilter.setParams(0.2f, 0.6f);                                           // 回声：延迟 0.2s，衰减 0.6
		mLofiFilter.setParams(8000, 8);                                              // LoFi：采样率 8000Hz，位深 8bit

		// ---- 将滤波器挂载到总线 ----
		mEnvironmentBus.setFilter(0, &mReverbFilter);   // 环境声总线 → 混响
		mEnvironmentBus.setFilter(1, &mEchoFilter);      // 环境声总线 → 回声
		mMusicBus.setFilter(0, &mMusicLPF);              // 音乐总线 → 低通
		mSfxBus.setFilter(0, &mLofiFilter);              // 音效总线 → LoFi

		// ---- 启动三条总线（总线必须先 play 才能接收子音频）----
		mEnvironmentBusHandle = mSoloud->play(mEnvironmentBus);
		mMusicBusHandle = mSoloud->play(mMusicBus);
		mSfxBusHandle = mSoloud->play(mSfxBus);

		mSoloud->setVolume(mEnvironmentBusHandle, 0.6f);
		mSoloud->setVolume(mMusicBusHandle, mMusicVolume);
		mSoloud->setVolume(mSfxBusHandle, mSfxVolume);

		// ---- 初始化 SFXR 程序化音效 ----

		// 室外风声 — 基于 HURT 预设修改，低频噪声，循环播放
		mSfxrWind.loadPreset(SoLoud::Sfxr::HURT, 12345);
		mSfxrWind.mParams.wave_type = 3;                         // 噪声波形
		mSfxrWind.mParams.p_env_attack = 0.3f;
		mSfxrWind.mParams.p_env_sustain = 0.4f;
		mSfxrWind.mParams.p_env_decay = 0.3f;
		mSfxrWind.mParams.p_env_punch = 0.1f;
		mSfxrWind.mParams.p_base_freq = 0.15f * 44100 / 22050;  // 低基频
		mSfxrWind.mParams.p_freq_ramp = -0.1f;                   // 频率缓慢下降
		mSfxrWind.setLooping(true);
		mSfxrWind.setFilter(0, &mOcclusionLPF);                  // 挂载遮挡低通滤波

		// Room A 音乐 — 基于 POWERUP 预设修改，方波旋律，循环播放
		mSfxrMusic.loadPreset(SoLoud::Sfxr::POWERUP, 42);
		mSfxrMusic.mParams.wave_type = 2;                         // 方波
		mSfxrMusic.mParams.p_base_freq = 0.4f * 44100 / 22050;
		mSfxrMusic.mParams.p_env_attack = 0.1f;
		mSfxrMusic.mParams.p_env_sustain = 0.3f;
		mSfxrMusic.mParams.p_env_decay = 0.3f;
		mSfxrMusic.mParams.p_freq_ramp = -0.2f;
		mSfxrMusic.setLooping(true);
		mSfxrMusic.setFilter(0, &mOcclusionLPF);

		// Room B 水滴声 — 基于 COIN 预设修改，正弦波短促音，循环播放
		mSfxrWater.loadPreset(SoLoud::Sfxr::COIN, 99);
		mSfxrWater.mParams.wave_type = 1;                         // 正弦波
		mSfxrWater.mParams.p_base_freq = 0.6f * 44100 / 22050;
		mSfxrWater.mParams.p_env_sustain = 0.1f;
		mSfxrWater.mParams.p_env_decay = 0.2f;
		mSfxrWater.setLooping(true);
		mSfxrWater.setFilter(0, &mOcclusionLPF);

		// 走廊嗡嗡声 — 基于 BLIP 预设修改，极低频持续音，循环播放
		mSfxrHum.loadPreset(SoLoud::Sfxr::BLIP, 77);
		mSfxrHum.mParams.wave_type = 0;                           // 方波（默认）
		mSfxrHum.mParams.p_base_freq = 0.08f * 44100 / 22050;    // 极低基频
		mSfxrHum.mParams.p_env_attack = 0.2f;
		mSfxrHum.mParams.p_env_sustain = 0.5f;
		mSfxrHum.mParams.p_env_decay = 0.2f;
		mSfxrHum.setLooping(true);
		mSfxrHum.setFilter(0, &mOcclusionLPF);

		// 脚步声 — 基于 HURT 预设修改，极短促噪声
		mSfxrFootstep.loadPreset(SoLoud::Sfxr::HURT, 55);
		mSfxrFootstep.mParams.wave_type = 3;
		mSfxrFootstep.mParams.p_env_attack = 0.0f;               // 无起音
		mSfxrFootstep.mParams.p_env_sustain = 0.01f;             // 极短持续
		mSfxrFootstep.mParams.p_env_decay = 0.05f;               // 快速衰减
		mSfxrFootstep.mParams.p_env_punch = 0.4f;                // 强冲击感
		mSfxrFootstep.mParams.p_base_freq = 0.3f * 44100 / 22050;

		// 碰撞冲击声 — 基于 EXPLOSION 预设修改
		mSfxrImpact.loadPreset(SoLoud::Sfxr::EXPLOSION, 33);
		mSfxrImpact.mParams.p_base_freq = 0.2f * 44100 / 22050;
		mSfxrImpact.mParams.p_env_decay = 0.3f;

		// 区域过渡提示音 — BLIP 预设
		mSfxrBlip.loadPreset(SoLoud::Sfxr::BLIP, 88);

		// ---- 创建声源并播放 ----
		Vec2_ centrality = mMapStatic->centrality;

		mSoundSources.clear();

		// 声源 1：室外风声 — 位于建筑上方的室外区域
		{
			SoundSource src;
			src.name = u8"Outdoor Wind";
			src.position = Vec2_{5, 15} - centrality;
			src.sfxr = &mSfxrWind;
			src.baseVolume = 0.4f;
			src.maxDistance = SOUND_MAX_DISTANCE;
			src.active = false;
			src.color = {0.5f, 0.8f, 1.0f, 1.0f};
			src.wallCount = 0;
			src.handle = mEnvironmentBus.play(mSfxrWind, 0.0f, 0.0f);
			mSoloud->setPause(src.handle, false);
			mSoundSources.push_back(src);
		}

		// 声源 2：Room A 音乐 — 位于建筑左半部分上层
		{
			SoundSource src;
			src.name = u8"Room A Music";
			src.position = Vec2_{18, 13} - centrality;
			src.sfxr = &mSfxrMusic;
			src.baseVolume = 0.5f;
			src.maxDistance = SOUND_MAX_DISTANCE;
			src.active = false;
			src.color = {1.0f, 0.9f, 0.3f, 1.0f};
			src.wallCount = 0;
			src.handle = mMusicBus.play(mSfxrMusic, 0.0f, 0.0f);
			mSoloud->setPause(src.handle, false);
			mSoundSources.push_back(src);
		}

		// 声源 3：Room B 水滴声 — 位于建筑右半部分上层
		{
			SoundSource src;
			src.name = u8"Room B Water";
			src.position = Vec2_{28, 13} - centrality;
			src.sfxr = &mSfxrWater;
			src.baseVolume = 0.35f;
			src.maxDistance = SOUND_MAX_DISTANCE;
			src.active = false;
			src.color = {0.3f, 0.6f, 1.0f, 1.0f};
			src.wallCount = 0;
			src.handle = mEnvironmentBus.play(mSfxrWater, 0.0f, 0.0f);
			mSoloud->setPause(src.handle, false);
			mSoundSources.push_back(src);
		}

		// 声源 4：走廊嗡嗡声 — 位于走廊中央
		{
			SoundSource src;
			src.name = u8"Corridor Hum";
			src.position = Vec2_{23, 8} - centrality;
			src.sfxr = &mSfxrHum;
			src.baseVolume = 0.3f;
			src.maxDistance = 15.0f;    // 走廊声源可听距离较短
			src.active = false;
			src.color = {1.0f, 0.5f, 0.2f, 1.0f};
			src.wallCount = 0;
			src.handle = mEnvironmentBus.play(mSfxrHum, 0.0f, 0.0f);
			mSoloud->setPause(src.handle, false);
			mSoundSources.push_back(src);
		}
	}

	// ==================== 环境检测 ====================

	// 判断某位置是否在室内：从该位置向上射线，如果遇到任何 Entity 格子则认为有天花板，即为室内
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

	// 统计两点之间穿过的墙壁数量
	// 使用射线采样：沿 from→to 方向等间距采样，检测每个采样点是否为碰撞格
	// 相邻的连续墙壁只计为一次穿越（lastWasWall 去重）
	int SoloudTest::CountWallsBetween(Vec2_ from, Vec2_ to)
	{
		Vec2_ mapFrom = from + mMapStatic->centrality;
		Vec2_ mapTo = to + mMapStatic->centrality;

		int wallCount = 0;
		// 采样步数 = 距离 × 2，保证每半格至少一个采样点
		int steps = (int)(PhysicsBlock::Modulus(mapTo - mapFrom) * 2) + 1;
		if (steps < 2) steps = 2;
		if (steps > 200) steps = 200;   // 上限防止性能问题

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

	// 判断玩家是否站在地面上：检测玩家脚下（圆心下方 radius+0.2）是否有碰撞块
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

	// 播放脚步声：室内时音量略低（模拟室内地面吸音）
	void SoloudTest::PlayFootstep()
	{
		float vol = mSfxVolume * 0.5f;
		if (mIndoorBlend > 0.5f) {
			vol *= 0.7f;
		}
		mSfxBus.play(mSfxrFootstep, vol, 0);
	}

	// 播放碰撞冲击声：intensity 为速度突变强度，映射到 0.1~1.0 音量范围
	void SoloudTest::PlayImpactSound(float intensity)
	{
		float vol = glm::clamp(intensity / 50.0f, 0.1f, 1.0f) * mSfxVolume;
		mSfxBus.play(mSfxrImpact, vol, 0);
	}

	// 播放区域过渡提示音：进入室内时音调较低 (0.8x)，离开室内时音调较高 (1.3x)
	void SoloudTest::PlayZoneTransitionSound(bool enteringIndoor)
	{
		float vol = mSfxVolume * 0.3f;
		SoLoud::handle h = mSfxBus.play(mSfxrBlip, vol, 0);
		if (enteringIndoor) {
			mSoloud->setRelativePlaySpeed(h, 0.8f);
		}
		else {
			mSoloud->setRelativePlaySpeed(h, 1.3f);
		}
	}

	// ==================== 音频状态更新（每帧调用）====================
	//
	// 更新流程：
	//   1. 检测室内/室外状态，平滑过渡 indoorBlend
	//   2. 检测走廊状态，平滑过渡 corridorBlend
	//   3. 根据混合因子更新总线滤波器参数（混响、回声、LoFi）
	//   4. 遍历所有声源，计算距离衰减、遮挡衰减、声像、低通截止频率
	//   5. 更新总线音量
	//   6. 检测脚步声和碰撞冲击声

	void SoloudTest::UpdateAudio()
	{
		if (!mPlayer || !mSoloud) return;

		Vec2_ playerPos = mPlayer->pos;
		bool isIndoor = IsPositionIndoor(playerPos);

		// 室内/室外状态切换时播放过渡提示音
		if (isIndoor != mLastIndoorState) {
			PlayZoneTransitionSound(isIndoor);
			mLastIndoorState = isIndoor;
		}

		// 平滑过渡室内混合因子（指数衰减插值，5.0 为过渡速度）
		float targetIndoor = isIndoor ? 1.0f : 0.0f;
		float blendFactor = (float)glm::min((double)(TOOL::FPStime * 5.0f), 1.0);
		mIndoorBlend += (targetIndoor - mIndoorBlend) * blendFactor;

		// 检测是否在走廊区域（地图坐标 x=15~31, y=7~9）
		Vec2_ mapPos = playerPos + mMapStatic->centrality;
		bool inCorridor = (mapPos.x >= 15 && mapPos.x <= 31 && mapPos.y >= 7 && mapPos.y <= 9);
		float targetCorridor = inCorridor ? 1.0f : 0.0f;
		blendFactor = (float)glm::min((double)(TOOL::FPStime * 5.0f), 1.0);
		mCorridorBlend += (targetCorridor - mCorridorBlend) * blendFactor;

		// 根据混合因子计算滤波器参数
		mReverbWet = mIndoorBlend * 0.7f;        // 室内时混响湿信号最大 0.7
		mReverbRoomSize = mIndoorBlend * 0.8f;    // 室内时房间大小最大 0.8
		mEchoWet = mCorridorBlend * 0.5f;         // 走廊时回声湿信号最大 0.5
		mLofiWet = mCorridorBlend * 0.3f;         // 走廊时 LoFi 湿信号最大 0.3

		// 使用 fadeFilterParameter 平滑过渡滤波器参数（0.1s 过渡时间）
		mSoloud->fadeFilterParameter(mEnvironmentBusHandle, 0,
			SoLoud::FreeverbFilter::WET, mReverbWet, 0.1f);
		mSoloud->fadeFilterParameter(mEnvironmentBusHandle, 0,
			SoLoud::FreeverbFilter::ROOMSIZE, mReverbRoomSize, 0.1f);
		mSoloud->fadeFilterParameter(mEnvironmentBusHandle, 1,
			SoLoud::EchoFilter::WET, mEchoWet, 0.1f);
		mSoloud->fadeFilterParameter(mSfxBusHandle, 0,
			SoLoud::LofiFilter::WET, mLofiWet, 0.1f);

		// 遍历所有声源，更新音量、声像、遮挡低通
		for (auto &src : mSoundSources) {
			// 距离衰减：使用反平方衰减 1/(1+d²×0.02)
			float distance = (float)PhysicsBlock::Modulus(playerPos - src.position);
			float attenuation = 1.0f / (1.0f + distance * distance * 0.02f);
			attenuation = glm::clamp(attenuation, 0.0f, 1.0f);

			// 超过最大可听距离则静音
			if (distance > src.maxDistance) {
				attenuation = 0.0f;
			}

			// 遮挡计算：统计玩家到声源之间穿过的墙壁数量
			int walls = CountWallsBetween(playerPos, src.position);
			src.wallCount = walls;

			// 遮挡衰减：墙壁越多音量越低 1/(1+walls×0.8)
			float occlusionFactor = 1.0f;
			// 遮挡低通：墙壁越多截止频率越低 22000/(1+walls×3)，最低 400Hz
			float lpfFreq = 22000.0f;
			if (walls > 0) {
				occlusionFactor = 1.0f / (1.0f + walls * 0.8f);
				lpfFreq = glm::max(400.0f, 22000.0f / (1.0f + walls * 3.0f));
			}

			// 最终音量 = 基础音量 × 距离衰减 × 遮挡衰减 × 主音量
			float volume = src.baseVolume * attenuation * occlusionFactor * mMasterVolume;
			mSoloud->setVolume(src.handle, volume);

			// 立体声声像：声源在玩家左方 pan<0，右方 pan>0
			float pan = (float)((src.position.x - playerPos.x) / src.maxDistance);
			pan = glm::clamp(pan, -1.0f, 1.0f);
			mSoloud->setPan(src.handle, pan);

			// 平滑过渡遮挡低通截止频率（0.15s 过渡时间）
			mSoloud->fadeFilterParameter(src.handle, 0,
				SoLoud::BiquadResonantFilter::FREQUENCY, lpfFreq, 0.15f);
		}

		// 更新总线音量
		mSoloud->setVolume(mEnvironmentBusHandle, 0.6f * mMasterVolume);
		mSoloud->setVolume(mMusicBusHandle, mMusicVolume * mMasterVolume);
		mSoloud->setVolume(mSfxBusHandle, mSfxVolume * mMasterVolume);

		// 脚步声检测：速度 > 1 且在地面时，按速度间隔触发
		float speed = (float)PhysicsBlock::Modulus(mPlayer->speed);
		if (speed > 1.0f && IsPlayerOnGround()) {
			mFootstepTimer -= TOOL::FPStime;
			if (mFootstepTimer <= 0) {
				PlayFootstep();
				// 速度越快脚步间隔越短，最短 0.2s
				mFootstepTimer = glm::max(0.2f, 0.5f - speed * 0.02f);
			}
		}
		else {
			mFootstepTimer = 0;
		}

		// 碰撞冲击检测：速度突变 > 8 时触发冲击音效
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

	// 鼠标滚轮：缩放相机（Z 轴距离）
	void SoloudTest::MouseRoller(int z)
	{
		if (Global::ClickWindow) return;

		glm::vec3 camPos = mCamera->getCameraPos();
		if (camPos.z <= 10) {
			camPos.z += (camPos.z / 2) * z;   // 近距离时小步缩放
		}
		else {
			camPos.z += z * 5;                  // 远距离时大步缩放
		}
		if (camPos.z <= 0.1f) camPos.z = 0.1f;
		mCameraTarget.z = camPos.z;
	}

	// 键盘事件
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
			if (IsPlayerOnGround()) {
				mPlayer->AddForce({0, PLAYER_JUMP_FORCE});   // 仅在地面时可跳跃
			}
			break;
		case GameKeyEnum::MOVE_BACK:
			mPlayer->AddForce({0, -PLAYER_FORCE * 0.5});
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

		// 固定时间步长物理仿真 (100Hz)，防止帧率波动影响物理稳定性
		if (mPhysicsSwitch) {
			#define PhysicsTick (1.0 / 100.0)
			static float AddUpTime = 0;
			AddUpTime += TOOL::FPStime;
			if (AddUpTime > PhysicsTick) {
				AddUpTime -= PhysicsTick;
				mPhysicsWorld->PhysicsEmulator(PhysicsTick);
				if (AddUpTime > 1.0) AddUpTime = 0.1;   // 防止累积延迟
			}
		}
		else {
			// 物理暂停时仅更新碰撞信息（不移动物体）
			mPhysicsWorld->PhysicsInformationUpdate();
		}

		// 每帧更新音频状态
		UpdateAudio();

		// 渲染场景
		RenderScene();

		mAuxiliaryVision->End();

		// 相机平滑跟随玩家
		if (mPlayer) {
			glm::vec3 currentPos = mCamera->getCameraPos();
			glm::vec3 targetPos = {(float)mPlayer->pos.x, (float)mPlayer->pos.y, mCameraTarget.z};
			float cameraBlendFactor = (float)glm::min((double)(TOOL::FPStime * 5.0f), 1.0);
			glm::vec3 newPos = glm::mix(currentPos, targetPos, cameraBlendFactor);
			mCamera->setCameraPos(newPos);
		}

		// 更新 VP 矩阵到 GPU Uniform Buffer
		mCamera->update();
		VPMatrices *mVPMatrices = (VPMatrices *)mCameraVPMatricesBuffer[mCurrentFrame]->getupdateBufferByMap();
		mVPMatrices->mViewMatrix = mCamera->getViewMatrix();
		mCameraVPMatricesBuffer[mCurrentFrame]->endupdateBufferByMap();
	}

	// ==================== 渲染 ====================

	void SoloudTest::RenderScene()
	{
		if (!mPhysicsWorld) return;

		// 渲染网格形状（PhysicsShape）— 绿色线框
		for (auto i : mPhysicsWorld->GetPhysicsShape()) {
			for (size_t x = 0; x < i->width; ++x) {
				for (size_t y = 0; y < i->height; ++y) {
					if (i->at(x, y).Entity) {
						ShowSquare(
							PhysicsBlock::vec2angle(Vec2_{(FLOAT_)x, (FLOAT_)y} - i->CentreMass, i->angle) + i->pos,
							i->angle,
							{0, (i->StaticNum < 10 ? 1 : 0.2), 0, 1});   // 静止时变暗
					}
				}
			}
		}

		// 渲染粒子（PhysicsParticle）— 绿色小点
		for (auto i : mPhysicsWorld->GetPhysicsParticle()) {
			mAuxiliaryVision->Spot({i->pos, 0}, 0.05f, {0, (i->StaticNum < 10 ? 1 : 0.2), 0, 1});
		}

		// 渲染圆形（PhysicsCircle）— 玩家蓝色，其他绿色
		for (auto i : mPhysicsWorld->GetPhysicsCircle()) {
			glm::vec4 color;
			if (i == mPlayer) {
				color = {0.3f, 0.5f, 1.0f, 1.0f};
			}
			else {
				color = {0, (i->StaticNum < 10 ? 1 : 0.2), 0, 1};
			}
			mAuxiliaryVision->Circle({i->pos, 0}, i->radius, color);

			// 玩家额外渲染朝向指示线
			if (i == mPlayer) {
				mAuxiliaryVision->Line({i->pos, 0}, {1, 1, 0, 1},
					{i->pos + PhysicsBlock::vec2angle({i->radius, 0}, i->angle), 0}, {1, 1, 0, 1});
			}
		}

		// 渲染线段（PhysicsLine）— 绿色
		for (auto i : mPhysicsWorld->GetPhysicsLine()) {
			Vec2_ pR = PhysicsBlock::vec2angle({i->radius, 0}, i->angle);
			mAuxiliaryVision->Line({i->pos + pR, 0}, {0, 1, 0, 1}, {i->pos - pR, 0}, {0, 1, 0, 1});
		}

		// 渲染关节（PhysicsJoint）— 绿色连接线
		for (auto i : mPhysicsWorld->GetPhysicsJoint()) {
			mAuxiliaryVision->Line({i->body1->pos, 0}, {0, 1, 0, 1}, {i->body1->pos + i->r1, 0}, {0, 1, 0, 1});
			mAuxiliaryVision->Line({i->body2->pos, 0}, {0, 1, 0, 1}, {i->body2->pos + i->r2, 0}, {0, 1, 0, 1});
		}
		// 渲染绳索/弹簧（BaseJunction）— 绿色连接线
		for (auto j : mPhysicsWorld->GetBaseJunction()) {
			mAuxiliaryVision->Line({j->GetA(), 0}, {0, 1, 0, 1}, {j->GetB(), 0}, {0, 1, 0, 1});
		}

		// 渲染声源标记
		RenderSoundSources();
		// 渲染音频调试可视化
		if (mDebugVisualization) {
			RenderAudioDebug();
		}
	}

	// 渲染声源位置标记和声音半径圈
	void SoloudTest::RenderSoundSources()
	{
		for (auto &src : mSoundSources) {
			// 声源中心点
			mAuxiliaryVision->Spot({src.position, 0}, 0.15f, src.color);

			// 脉冲动画圆圈 — 模拟声音传播的视觉效果
			float pulsePhase = fmod((float)glfwGetTime() * 2.0f, 1.0f);
			glm::vec4 pulseColor = src.color * (0.5f + pulsePhase * 0.5f);
			pulseColor.a = 0.5f;
			mAuxiliaryVision->Circle({src.position, 0}, 0.3f, pulseColor);

			// 声音最大可听距离圈
			if (mShowSoundRadius) {
				glm::vec4 radiusColor = src.color;
				radiusColor.a = 0.2f;
				mAuxiliaryVision->Circle({src.position, 0}, src.maxDistance, radiusColor);
			}
		}
	}

	// 渲染音频调试信息
	void SoloudTest::RenderAudioDebug()
	{
		if (!mPlayer) return;

		Vec2_ playerPos = mPlayer->pos;

		// 遮挡射线：绿色=无遮挡，红色=有墙壁遮挡
		for (auto &src : mSoundSources) {
			if (mShowOcclusionRays) {
				glm::vec4 rayColor = src.wallCount > 0 ? glm::vec4{1, 0.2, 0.2, 0.6} : glm::vec4{0.2, 1, 0.2, 0.6};
				mAuxiliaryVision->Line({playerPos, 0}, rayColor, {src.position, 0}, rayColor);
			}
		}

		// 室内指示圈：蓝色半透明圆圈，透明度随 indoorBlend 增大
		if (mIndoorBlend > 0.1f) {
			glm::vec4 indoorColor = {0.2, 0.5, 1.0, mIndoorBlend * 0.3f};
			mAuxiliaryVision->Circle({playerPos, 0}, 1.5f, indoorColor);
		}
	}

	// ==================== ImGui UI ====================

	void SoloudTest::GameUI()
	{
		ImGui::Begin(u8"SoLoud Audio Demo");

		// 检测鼠标是否在 ImGui 窗口上，避免同时触发游戏操作
		ImVec2 window_pos = ImGui::GetWindowPos();
		ImVec2 window_size = ImGui::GetWindowSize();
		if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
			((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY))) {
			Global::ClickWindow = true;
		}

		// 音量控制滑块
		ImGui::Text(u8"--- Volume Control ---");
		ImGui::SliderFloat(u8"Master", &mMasterVolume, 0.0f, 1.0f);
		ImGui::SliderFloat(u8"Music", &mMusicVolume, 0.0f, 1.0f);
		ImGui::SliderFloat(u8"SFX", &mSfxVolume, 0.0f, 1.0f);

		// 环境状态显示
		ImGui::Separator();
		ImGui::Text(u8"--- Environment Status ---");

		const char *locationStr = mIndoorBlend > 0.5f ? u8"Indoor" : u8"Outdoor";
		ImGui::Text(u8"Location: %s", locationStr);
		ImGui::Text(u8"Indoor Blend: %.2f", mIndoorBlend);
		ImGui::Text(u8"Corridor Blend: %.2f", mCorridorBlend);

		// 音频效果参数显示
		ImGui::Separator();
		ImGui::Text(u8"--- Audio Effects ---");
		ImGui::Text(u8"Reverb Wet: %.2f", mReverbWet);
		ImGui::Text(u8"Reverb RoomSize: %.2f", mReverbRoomSize);
		ImGui::Text(u8"Echo Wet: %.2f", mEchoWet);
		ImGui::Text(u8"LoFi Wet: %.2f", mLofiWet);

		// 声源列表：距离、遮挡墙壁数、当前音量
		ImGui::Separator();
		ImGui::Text(u8"--- Sound Sources ---");
		for (auto &src : mSoundSources) {
			float dist = mPlayer ? (float)PhysicsBlock::Modulus(mPlayer->pos - src.position) : 0;
			float vol = 0;
			if (mSoloud->isValidVoiceHandle(src.handle)) {
				vol = mSoloud->getVolume(src.handle);
			}
			ImGui::PushStyleColor(ImGuiCol_Text, {src.color.r, src.color.g, src.color.b, src.color.a});
			ImGui::Text(u8"%s", src.name.c_str());
			ImGui::PopStyleColor();
			ImGui::SameLine();
			ImGui::Text(u8" Dist:%.1f Walls:%d Vol:%.2f", dist, src.wallCount, vol);
		}

		// 调试开关
		ImGui::Separator();
		ImGui::Text(u8"--- Debug ---");
		ImGui::Checkbox(u8"Debug Visualization", &mDebugVisualization);
		ImGui::Checkbox(u8"Show Sound Radius", &mShowSoundRadius);
		ImGui::Checkbox(u8"Show Occlusion Rays", &mShowOcclusionRays);
		if (ImGui::Button(mPhysicsSwitch ? u8"Pause Physics" : u8"Resume Physics")) {
			mPhysicsSwitch = !mPhysicsSwitch;
		}

		// 操作提示
		ImGui::Separator();
		ImGui::Text(u8"--- Controls ---");
		ImGui::Text(u8"A/D: Move  W: Jump  S: Down");
		ImGui::Text(u8"1: Toggle Debug  2: Toggle Physics");
		ImGui::Text(u8"Scroll: Zoom");

		// 玩家状态
		if (mPlayer) {
			ImGui::Separator();
			ImGui::Text(u8"Player Pos: (%.1f, %.1f)", mPlayer->pos.x, mPlayer->pos.y);
			ImGui::Text(u8"Player Speed: (%.1f, %.1f)", mPlayer->speed.x, mPlayer->speed.y);
			ImGui::Text(u8"On Ground: %s", IsPlayerOnGround() ? u8"Yes" : u8"No");
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

	// TCP 网络事件循环
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

	// 绘制静态方块（地图墙壁）— 使用 AddStaticLine 一次性录制，不会每帧重新录制
	void SoloudTest::ShowStaticSquare(glm::dvec2 pos, double angle, glm::vec4 color)
	{
		glm::dvec2 Angle = PhysicsBlock::AngleFloatToAngleVec(angle);
		glm::dvec2 jiao1 = PhysicsBlock::vec2angle({0, 1}, Angle);   // 上方向角点偏移
		glm::dvec2 jiao2 = PhysicsBlock::vec2angle({1, 0}, Angle);   // 右方向角点偏移
		glm::dvec2 jiao3 = jiao2 + jiao1;                            // 右上角点偏移
		mAuxiliaryVision->AddStaticLine({pos, 0}, {pos + jiao1, 0}, color);
		mAuxiliaryVision->AddStaticLine({pos, 0}, {pos + jiao2, 0}, color);
		mAuxiliaryVision->AddStaticLine({pos + jiao3, 0}, {pos + jiao1, 0}, color);
		mAuxiliaryVision->AddStaticLine({pos + jiao3, 0}, {pos + jiao2, 0}, color);
	}

	// 绘制动态方块（物理对象）— 使用 Line 每帧重新录制
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
