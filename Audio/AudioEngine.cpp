// 音频引擎实现。
// 初始化链路：SoLoud::init → MIDI SoundFont → AudioBank → BusSystem（6 条总线）
// → VoiceManager（128 最大声部）→ SpatialAudioSystem → RoomAcoustics → RTPC
// 每帧 Update 驱动：BusSystem 淡入淡出 → VoiceManager 时间步进 → RoomAcoustics 平滑过渡
#include "AudioEngine.h"
#include "AudioAsset.h"
#include "AudioBus.h"
#include "AudioVoice.h"
#include "SpatialAudio.h"
#include "RoomAcoustics.h"
#include "RTPCController.h"
#include "../DebugLog.h"
#include <FilePath.h>

namespace GAME::Audio {

AudioEngine& AudioEngine::Get()
{
    static AudioEngine instance;
    return instance;
}

bool AudioEngine::Initialize()
{
    if (mInitialized)
        return true;

    LOGD("[AudioEngine] Initializing...");

    // 1. SoLoud 内核初始化
    SoLoud::result res = mSoloud.init();
    if (res != SoLoud::SO_NO_ERROR)
    {
        LOGE("[AudioEngine] mSoloud.init() failed: %d", res);
        return false;
    }

    // 2. 加载 MIDI 音色库（失败仅警告，不影响 MP3 播放）
    if (mMidiFont.load(TimGM6mb_sf2) != 0)
    {
        LOGE("[AudioEngine] Warning: failed to load SoundFont: %s", TimGM6mb_sf2);
        mMidiFontLoaded = false;
    }
    else
    {
        mMidiFontLoaded = true;
    }

    // 3. 资源库
    mBank = new AudioBank();

    // 4. 混音总线系统（6 条总线，Environment/Music/SFX/Voice/UI/Master）
    mBusSystem = new BusSystem();
    if (!mBusSystem->Initialize(&mSoloud))
    {
        LOGE("[AudioEngine] BusSystem initialization failed");
        return false;
    }

    // 5. 声部管理器（128 最大声部，支持优先级偷取）
    mVoiceManager = new VoiceManager();
    mVoiceManager->Initialize(128);

    // 6. 空间音频系统（3D 声源 + 遮挡模拟）
    mSpatialAudio = new SpatialAudioSystem();
    if (!mSpatialAudio->Initialize(&mSoloud, mBusSystem, mBank, 128))
    {
        LOGE("[AudioEngine] SpatialAudioSystem initialization failed");
        return false;
    }

    // 7. 房间声学检测器 + 效果器（混响/回声自动应用）
    mRoomDetector = new RoomAcousticsDetector();
    mRoomApplier = new RoomAcousticsApplier();
    mRoomApplier->Initialize(&mSoloud, mBusSystem);

    // 8. 运行时参数控制器
    mRTPC = new RTPCController();

    mInitialized = true;
    LOGD("[AudioEngine] Initialization complete");
    return true;
}

void AudioEngine::Shutdown()
{
    if (!mInitialized)
        return;

    LOGD("[AudioEngine] Shutting down...");

    // 停掉所有正在播放的声音
    mSoloud.stopAll();

    // 逆序删除子系统
    delete mRTPC;
    delete mRoomApplier;
    delete mRoomDetector;
    delete mSpatialAudio;
    delete mVoiceManager;
    delete mBusSystem;

    if (mBank)
    {
        mBank->UnloadAll();
        delete mBank;
    }

    // 关闭 SoLoud 内核
    mSoloud.deinit();

    mRTPC = nullptr;
    mRoomApplier = nullptr;
    mRoomDetector = nullptr;
    mSpatialAudio = nullptr;
    mVoiceManager = nullptr;
    mBusSystem = nullptr;
    mBank = nullptr;
    mInitialized = false;

    LOGD("[AudioEngine] Shutdown complete");
}

void AudioEngine::Update(float deltaTime)
{
    if (!mInitialized)
        return;

    // 驱动总线淡入淡出
    mBusSystem->Update(deltaTime);
    // 驱动声部时间步进
    mVoiceManager->Update();
    // 驱房间声学效果平滑过渡
    mRoomApplier->Update(deltaTime);
}

AudioEngine::~AudioEngine()
{
    Shutdown();
}

} // namespace GAME::Audio