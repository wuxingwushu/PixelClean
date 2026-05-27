// 音频引擎顶层入口。全局单例，管理所有音频子系统的生命周期。
// 初始化顺序：SoLoud 引擎 → 资源库 → 6 条混音总线 → 声部管理器 → 空间音频 → 房间声学 → RTPC
// 每帧调用 Update(deltaTime) 驱动淡入淡出和房间效果平滑过渡。
#pragma once
#include "soloud.h"
#include "soloud_bus.h"
#include "soloud_midi.h"
#include <string>

namespace GAME::Audio {

class AudioBank;
class BusSystem;
class VoiceManager;
class SpatialAudioSystem;
class RoomAcousticsDetector;
class RoomAcousticsApplier;
class RTPCController;

class AudioEngine
{
public:
    // 获取全局唯一实例（线程安全的静态局部变量初始化）
    static AudioEngine& Get();

    // 初始化 SoLoud 内核 + 全部子系统。成功返回 true。
    // 可重复调用（mInitialized 标志保护）。
    bool Initialize();
    // 反初始化：停声 → 删子系统 → deinit SoLoud
    void Shutdown();
    // 每帧更新，驱动总线淡入淡出、声部管理和房间声学平滑过渡
    void Update(float deltaTime);

    // ---- 子系统访问器 ----
    AudioBank& GetBank() { return *mBank; }
    BusSystem& GetBusSystem() { return *mBusSystem; }
    VoiceManager& GetVoiceManager() { return *mVoiceManager; }
    SpatialAudioSystem& GetSpatial() { return *mSpatialAudio; }
    RoomAcousticsDetector& GetRoomDetector() { return *mRoomDetector; }
    RoomAcousticsApplier& GetRoomApplier() { return *mRoomApplier; }
    RTPCController& GetRTPC() { return *mRTPC; }

    // 底层 SoLoud 引擎引用，供兼容层和高级用户直接操作
    SoLoud::Soloud& GetSoloud() { return mSoloud; }
    // MIDI 音色库（TimGM6mb.sf2），加载 MIDI 资源时使用
    SoLoud::SoundFont& GetMidiFont() { return mMidiFont; }

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

private:
    AudioEngine() = default;
    ~AudioEngine();

    SoLoud::Soloud mSoloud;
    SoLoud::SoundFont mMidiFont;
    bool mInitialized = false;

    AudioBank* mBank = nullptr;
    BusSystem* mBusSystem = nullptr;
    VoiceManager* mVoiceManager = nullptr;
    SpatialAudioSystem* mSpatialAudio = nullptr;
    RoomAcousticsDetector* mRoomDetector = nullptr;
    RoomAcousticsApplier* mRoomApplier = nullptr;
    RTPCController* mRTPC = nullptr;
};

} // namespace GAME::Audio