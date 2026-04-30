#pragma once

/*
 * SoloudTest — 基于 PhysicsBlock 物理系统 + SoLoud 音频引擎的 2D 环境音效 Demo
 *
 * 核心演示内容：
 *   1. 室内/室外环境检测 — 从玩家位置向上射线判断是否有天花板
 *   2. 墙壁遮挡模拟 — 从玩家到声源射线采样，统计穿过的墙壁数量，据此降低音量和低通滤波频率
 *   3. 室内混响 (FreeverbFilter) — 进入室内时混响 Wet/RoomSize 平滑增大
 *   4. 走廊回声 (EchoFilter) — 在走廊区域回声 Wet 增大
 *   5. 遮挡低通滤波 (BiquadResonantFilter LOWPASS) — 每个声源挂载 LPF，墙壁越多截止频率越低
 *   6. 走廊 LoFi 效果 (LofiFilter) — SFX Bus 挂载 LoFi，模拟走廊中的收音机/电话音效
 *   7. 距离衰减 — 手动计算 1/(1+d²×0.02) 衰减曲线，超过最大距离静音
 *   8. 立体声声像 (Pan) — 根据声源相对玩家的水平偏移设置左右声道
 *   9. 脚步声 — SFXR 生成，行走时按速度间隔触发，室内音量略低
 *  10. 碰撞冲击声 — 检测速度突变时触发 EXPLOSION 音效
 *  11. 区域过渡音效 — 进入/离开室内时播放 BLIP 音效，音调不同
 *  12. 三条音频总线 — Environment Bus / Music Bus / SFX Bus，分别控制音量和滤波
 *
 * 场景布局 (40×25 侧视图，重力向下)：
 *   ┌──────────────────────────────────────┐
 *   │            室外区域 (Outdoor)          │
 *   │            ← 风声声源 →               │
 *   │         ┌──────────────┐              │
 *   │         │ RoomA │RoomB │              │
 *   │         │ 音乐  │ 水滴  │              │
 *   │         │  门   │  门   │              │
 *   │         │门 ─── ┤      │              │
 *   │         │   走廊(回声)  │              │
 *   │         └──────────────┘              │
 *   │  玩家●    ●球   ■方块                 │
 *   └──────────────────────────────────────┘
 *
 * 操作方式：
 *   A/D — 左右移动    W — 跳跃    S — 向下施力
 *   1 — 切换调试可视化    2 — 暂停/恢复物理
 *   滚轮 — 缩放相机
 */

#include "../Configuration.h"
#include "../GameMods.h"
#include "../../PhysicsBlock/PhysicsWorld.hpp"
#include "../../PhysicsBlock/MapStatic.hpp"
#include "../../SoundEffect/SoundEffect.h"
#include "soloud.h"
#include "soloud_wav.h"
#include "soloud_bus.h"
#include "soloud_freeverbfilter.h"
#include "soloud_biquadresonantfilter.h"
#include "soloud_echofilter.h"
#include "soloud_lofifilter.h"
#include "soloud_sfxr.h"
#include <vector>

namespace GAME
{

	class SoloudTest : public GameMods, Configuration
	{
	public:
		SoloudTest(Configuration wConfiguration);
		~SoloudTest();

		virtual void MouseMove(double xpos, double ypos);
		virtual void MouseRoller(int z);
		virtual void KeyDown(GameKeyEnum moveDirection);
		virtual void GameCommandBuffers(unsigned int Format_i);
		virtual void GameLoop(unsigned int mCurrentFrame);
		virtual void GameRecordCommandBuffers();
		virtual void GameStopInterfaceLoop(unsigned int mCurrentFrame);
		virtual void GameTCPLoop();
		virtual void GameUI();

	private:

		// 声源描述结构体，记录每个环境声源的位置、音量、遮挡等信息
		struct SoundSource {
			std::string name;         // 声源名称（用于 UI 显示）
			Vec2_ position;           // 声源在世界坐标中的位置
			SoLoud::Sfxr* sfxr;      // 指向成员 Sfxr 对象的指针（不拥有所有权）
			SoLoud::handle handle;    // SoLoud 播放句柄，用于控制音量/滤波/声像
			float baseVolume;         // 基础音量（无衰减无遮挡时的参考音量）
			float maxDistance;        // 最大可听距离，超过此距离静音
			bool active;              // 是否激活（预留）
			glm::vec4 color;          // 调试渲染时的标识颜色
			int wallCount;            // 当前帧从玩家到声源之间穿过的墙壁数量
		};

		// 初始化音频系统：滤波器、总线、SFXR 音效、声源
		void InitAudio();
		// 创建物理场景：地图、建筑、玩家、装饰物体
		void CreateScene();
		// 每帧更新音频状态：室内/室外检测、遮挡计算、滤波器参数、脚步/碰撞音效
		void UpdateAudio();
		// 渲染物理对象（形状、粒子、圆形、线段、关节、绳索）
		void RenderScene();
		// 渲染声源位置标记和声音半径圈
		void RenderSoundSources();
		// 渲染音频调试信息：遮挡射线、室内指示圈
		void RenderAudioDebug();

		// 判断某世界坐标位置是否在室内（向上射线检测天花板）
		bool IsPositionIndoor(Vec2_ pos);
		// 统计两点之间穿过的墙壁数量（射线采样，相邻墙壁只计一次）
		int CountWallsBetween(Vec2_ from, Vec2_ to);
		// 判断玩家是否站在地面上（检测脚下是否有碰撞块）
		bool IsPlayerOnGround();
		// 播放脚步声音效（室内时音量略低）
		void PlayFootstep();
		// 播放碰撞冲击音效，intensity 为速度突变强度
		void PlayImpactSound(float intensity);
		// 播放区域过渡提示音，enteringIndoor=true 为进入室内，false 为离开
		void PlayZoneTransitionSound(bool enteringIndoor);

		// 绘制静态方块（地图墙壁，使用 AddStaticLine 一次性录制）
		void ShowStaticSquare(glm::dvec2 pos, double angle = 0, glm::vec4 color = {1, 0, 0, 1});
		// 绘制动态方块（物理对象，每帧重新录制 Line）
		void ShowSquare(glm::dvec2 pos, double angle = 0, glm::vec4 color = {1, 0, 0, 1});

	private:

		// ==================== 物理系统 ====================
		PhysicsBlock::PhysicsWorld *mPhysicsWorld = nullptr;   // 物理世界，驱动仿真
		PhysicsBlock::MapStatic *mMapStatic = nullptr;         // 静态地图（墙壁/建筑）
		PhysicsBlock::PhysicsCircle *mPlayer = nullptr;        // 玩家控制的圆形物理体
		VulKan::AuxiliaryVision *mAuxiliaryVision = nullptr;   // Vulkan 辅助线渲染器

		// ==================== SoLoud 音频引擎 ====================
		SoLoud::Soloud *mSoloud = nullptr;   // SoLoud 引擎指针（来自 SoundEffect 单例，不拥有所有权）

		// 三条音频总线，分别处理环境声、音乐、音效
		SoLoud::Bus mEnvironmentBus;   // 环境声总线（风声、水滴、嗡嗡声）— 挂载 Freeverb + Echo
		SoLoud::Bus mMusicBus;         // 音乐总线（房间音乐）— 挂载 BiquadResonant LPF
		SoLoud::Bus mSfxBus;           // 音效总线（脚步、碰撞、过渡提示）— 挂载 LofiFilter
		SoLoud::handle mEnvironmentBusHandle = 0;   // 环境声总线播放句柄
		SoLoud::handle mMusicBusHandle = 0;         // 音乐总线播放句柄
		SoLoud::handle mSfxBusHandle = 0;           // 音效总线播放句柄

		// 音频滤波器
		SoLoud::FreeverbFilter mReverbFilter;           // 混响滤波器 — 室内时增大 Wet/RoomSize
		SoLoud::BiquadResonantFilter mOcclusionLPF;     // 遮挡低通滤波器 — 挂载在每个声源上，墙壁越多截止频率越低
		SoLoud::BiquadResonantFilter mMusicLPF;         // 音乐低通滤波器 — 挂载在音乐总线上
		SoLoud::EchoFilter mEchoFilter;                 // 回声滤波器 — 走廊区域增大 Wet
		SoLoud::LofiFilter mLofiFilter;                 // LoFi 滤波器 — 走廊区域增大 Wet，模拟收音机/电话音效

		// 声源列表
		std::vector<SoundSource> mSoundSources;

		// SFXR 程序化音效生成器 — 一次性音效
		SoLoud::Sfxr mSfxrFootstep;   // 脚步声
		SoLoud::Sfxr mSfxrImpact;     // 碰撞冲击声
		SoLoud::Sfxr mSfxrBlip;       // 区域过渡提示音

		// SFXR 程序化音效生成器 — 循环环境声
		SoLoud::Sfxr mSfxrWind;       // 室外风声
		SoLoud::Sfxr mSfxrMusic;      // Room A 音乐
		SoLoud::Sfxr mSfxrWater;      // Room B 水滴声
		SoLoud::Sfxr mSfxrHum;        // 走廊嗡嗡声

		// ==================== 音频状态变量 ====================
		float mFootstepTimer = 0.0f;          // 脚步声计时器，控制脚步间隔
		bool mLastIndoorState = false;         // 上一帧的室内/室外状态，用于检测状态切换
		float mIndoorBlend = 0.0f;            // 室内混合因子 (0=室外, 1=室内)，平滑过渡
		float mCorridorBlend = 0.0f;          // 走廊混合因子 (0=非走廊, 1=走廊)，平滑过渡
		Vec2_ mPlayerPrevSpeed = {0, 0};      // 玩家上一帧速度，用于检测碰撞冲击

		// 音量控制
		float mMasterVolume = 1.0f;           // 主音量
		float mMusicVolume = 0.6f;            // 音乐音量
		float mSfxVolume = 0.8f;              // 音效音量

		// 滤波器参数（用于 UI 显示）
		float mReverbWet = 0.0f;              // 混响湿信号比例
		float mReverbRoomSize = 0.0f;         // 混响房间大小
		float mEchoWet = 0.0f;                // 回声湿信号比例
		float mLofiWet = 0.0f;                // LoFi 湿信号比例

		// ==================== 调试与控制 ====================
		bool mDebugVisualization = true;       // 是否显示音频调试可视化
		bool mPhysicsSwitch = true;            // 物理仿真开关（true=运行, false=暂停）
		bool mShowSoundRadius = true;          // 是否显示声源最大可听距离圈
		bool mShowOcclusionRays = true;        // 是否显示遮挡检测射线
		glm::vec3 mCameraTarget = {0, 0, 30}; // 相机目标位置（Z 分量控制缩放）

		// ==================== 常量 ====================
		static constexpr float PLAYER_FORCE = 60.0f;        // 玩家水平移动施力大小
		static constexpr float PLAYER_JUMP_FORCE = 250.0f;  // 玩家跳跃施力大小
		static constexpr float SOUND_MAX_DISTANCE = 25.0f;  // 默认声源最大可听距离
		static constexpr int MAP_WIDTH = 40;                 // 地图宽度（格子数）
		static constexpr int MAP_HEIGHT = 25;                // 地图高度（格子数）
	};

}
