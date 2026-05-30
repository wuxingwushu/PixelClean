// 空间音频系统实现。
// Play3D 流程：加载 Asset → 申请 VoiceInfo → 获取 AudioSource → play3d（带 BusHandle）
// → 配置衰减/多普勒 → 绑定 VoiceInfo。
// Play2D 流程：加载 Asset → 申请 VoiceInfo → play(paused) → 设置 handle → unpause。
// PlaySimple 流程：获取 Asset → 按类型设置循环 → play 直出 SoLoud（无总线路由）。
// 遮挡模拟：occlusion 0=无遮挡(22000Hz低通) → 1=完全遮挡(400Hz低通)。
#include "SpatialAudio.h"
#include "soloud_biquadresonantfilter.h"
#include "AudioEngine.h"
#include "../DebugLog.h"
#include "../Tool/Tool.h"
#include <algorithm>
#include <vector>

namespace GAME::Audio {

bool SpatialAudioSystem::Initialize(SoLoud::Soloud* soloud, BusSystem* busSystem,
                                     AudioBank* bank, int maxVoices)
{
    mSoloud = soloud;
    mBusSystem = busSystem;
    mBank = bank;
    mVoiceManager.Initialize(maxVoices);

    // 默认监听者：位置原点，面向前方（-Z），头上是 +Y
    mListener = {};
    mListener.upY = 1.0f;
    mListener.atZ = -1.0f;

    SetListener(mListener);

    LOGD("[SpatialAudio] Initialized with %d max voices", maxVoices);
    return true;
}

void SpatialAudioSystem::Shutdown()
{
    mSoloud = nullptr;
    mBusSystem = nullptr;
    mBank = nullptr;
}

void SpatialAudioSystem::SetListener(const Listener3DConfig& config)
{
    mListener = config;
    // 同步设置 SoLoud 引擎的监听者参数
    mSoloud->set3dListenerParameters(
        config.posX, config.posY, config.posZ,
        config.atX, config.atY, config.atZ,
        config.upX, config.upY, config.upZ,
        config.velX, config.velY, config.velZ);
}

void SpatialAudioSystem::SetListenerPosition(float x, float y, float z)
{
    mListener.posX = x;
    mListener.posY = y;
    mListener.posZ = z;
    mSoloud->set3dListenerPosition(x, y, z);
}

SoLoud::handle SpatialAudioSystem::Play3D(
    const std::string& assetName,
    const Source3DConfig& config,
    SoundPriority priority,
    BusType bus,
    float volume)
{
    AudioAsset* asset = mBank->Get(assetName);
    if (!asset)
    {
        LOGE("[SpatialAudio] Asset not found: %s", assetName.c_str());
        return 0;
    }

    VoiceInfo* voice = mVoiceManager.Allocate(priority);
    if (!voice)
        return 0;  // 声部已满且无法偷取

    // 根据资源类型获取 AudioSource 指针
    SoLoud::AudioSource* source = nullptr;
    switch (asset->type)
    {
        case AssetType::Sample: source = asset->wav; break;
        case AssetType::Midi:   source = asset->midi; break;
        case AssetType::SFXR:   source = asset->sfxr; break;
    }

    if (!source)
    {
        mVoiceManager.Free(0);
        return 0;
    }

    // 获取目标总线的 SoLoud 句柄，将 3D 声音路由到该总线
    unsigned int busHandle = static_cast<unsigned int>(
        mBusSystem->GetBusHandle(bus));

    // 调用 SoLoud 3D 播放 API
    SoLoud::handle h = mSoloud->play3d(
        *source,
        config.posX, config.posY, config.posZ,
        config.velX, config.velY, config.velZ,
        volume,
        false,     // 不暂停
        busHandle  // 路由到指定总线
    );

    if (h == 0)
    {
        mVoiceManager.Free(0);
        return 0;
    }

    // 配置 3D 声源的衰减参数
    mSoloud->set3dSourceMinMaxDistance(h, config.minDistance, config.maxDistance);
    mSoloud->set3dSourceAttenuation(h, 0, config.rolloffFactor);
    mSoloud->set3dSourceDopplerFactor(h, config.dopplerFactor);

    // 绑定声部信息
    voice->handle = h;
    voice->source = asset;
    voice->bus = bus;
    voice->priority = priority;
    voice->is3D = true;
    voice->currentVolume = volume;

    return h;
}

SoLoud::handle SpatialAudioSystem::Play2D(
    const std::string& assetName,
    float volume,
    SoundPriority priority,
    BusType bus)
{
    AudioAsset* asset = mBank->Get(assetName);
    if (!asset)
    {
        LOGE("[SpatialAudio] Asset not found: %s", assetName.c_str());
        return 0;
    }

    VoiceInfo* voice = mVoiceManager.Allocate(priority);
    if (!voice)
        return 0;

    SoLoud::AudioSource* source = nullptr;
    switch (asset->type)
    {
        case AssetType::Sample: source = asset->wav; break;
        case AssetType::Midi:   source = asset->midi; break;
        case AssetType::SFXR:   source = asset->sfxr; break;
    }

    if (!source)
    {
        mVoiceManager.Free(0);
        return 0;
    }

    unsigned int busHandle = static_cast<unsigned int>(
        mBusSystem->GetBusHandle(bus));

    // 先以暂停方式创建语音，设置好再 unpause，避免初始化时的杂音
    SoLoud::handle h = mSoloud->play(
        *source, volume, 0.0f, true, busHandle);

    if (h == 0)
    {
        mVoiceManager.Free(0);
        return 0;
    }

    mSoloud->setPause(h, false);

    voice->handle = h;
    voice->source = asset;
    voice->bus = bus;
    voice->priority = priority;
    voice->is3D = false;
    voice->currentVolume = volume;

    return h;
}

void SpatialAudioSystem::UpdateSource3D(SoLoud::handle handle, const Source3DConfig& config)
{
    mSoloud->set3dSourceParameters(handle,
        config.posX, config.posY, config.posZ,
        config.velX, config.velY, config.velZ);
}

void SpatialAudioSystem::SetSourceOcclusion(SoLoud::handle handle, float occlusion)
{
    occlusion = std::max(0.0f, std::min(1.0f, occlusion));

    // occlusion 0 → 22000Hz（无遮挡），occlusion 1 → 400Hz（完全遮挡）
    // 使用 BiquadResonantFilter 的 FREQUENCY 参数模拟低通截止频率
    float lpfFreq = 22000.0f * (1.0f - occlusion) + 400.0f * occlusion;

    mSoloud->setFilterParameter(handle, 0,
        SoLoud::BiquadResonantFilter::FREQUENCY, lpfFreq);
}

SoLoud::handle SpatialAudioSystem::PlayDirect(SoLoud::AudioSource& source,
                                               float volume, float pan,
                                               BusType bus)
{
    unsigned int busHandle = static_cast<unsigned int>(
        mBusSystem->GetBusHandle(bus));

    return mSoloud->play(source, volume, pan, false, busHandle);
}

void SpatialAudioSystem::Update(float /*deltaTime*/)
{
    mVoiceManager.Update();
}

SoLoud::handle SpatialAudioSystem::PlaySimple(
    const std::string& name,
    SimpleSoundType type,
    bool loop,
    float volume,
    float pan)
{
    AudioAsset* asset = mBank->Get(name);
    if (!asset)
    {
        LOGE("[SpatialAudio] PlaySimple asset not found: %s", name.c_str());
        return 0;
    }

    SoLoud::AudioSource* source = nullptr;
    switch (type)
    {
        case SimpleSoundType::MP3:  source = asset->wav;  break;
        case SimpleSoundType::MIDI:
            if (!AudioEngine::Get().IsMidiFontLoaded())
            {
                LOGE("[SpatialAudio] MIDI font not loaded, cannot play: %s", name.c_str());
                return 0;
            }
            source = asset->midi;
            break;
    }

    if (!source)
    {
        LOGE("[SpatialAudio] PlaySimple null source for: %s", name.c_str());
        return 0;
    }

    if (loop)
    {
        if (type == SimpleSoundType::MP3 && asset->wav)
            asset->wav->setLooping(true);
        else if (type == SimpleSoundType::MIDI && asset->midi)
            asset->midi->setLooping(true);
    }

    SoLoud::handle h = mSoloud->play(*source, volume, pan);

    if (loop)
    {
        if (type == SimpleSoundType::MP3 && asset->wav)
            asset->wav->setLooping(false);
        else if (type == SimpleSoundType::MIDI && asset->midi)
            asset->midi->setLooping(false);
    }

    return h;
}

void SpatialAudioSystem::Pause(SoLoud::handle handle, bool paused)
{
    if (handle == 0)
        mSoloud->setPauseAll(paused ? 1 : 0);
    else
        mSoloud->setPause(handle, paused ? 1 : 0);
}

void SpatialAudioSystem::StopAll()
{
    mSoloud->stopAll();
}

void SpatialAudioSystem::SetMasterVolume(float volume)
{
    mBusSystem->SetVolume(BusType::Master, volume);
}

void* SpatialAudioSystem::GetWav(const std::string& name)
{
    auto* asset = mBank->Get(name);
    if (asset && asset->type == AssetType::Sample)
        return asset->wav;
    return nullptr;
}

void SpatialAudioSystem::LoadAllResources()
{
    auto& engine = AudioEngine::Get();
    auto& bank = engine.GetBank();

    std::vector<std::string> SoundMp3;
    TOOL::FilePath("./Resource/Music", &SoundMp3, "mp3", "nullptr", nullptr);

    for (size_t i = 0; i < SoundMp3.size(); ++i)
    {
        std::string filePath = "./Resource/Music/" + SoundMp3[i] + ".mp3";
        auto* asset = bank.LoadSample(SoundMp3[i], filePath);
        if (!asset)
        {
            LOGE("[SpatialAudio] Failed to load MP3: %s", SoundMp3[i].c_str());
        }
    }

    std::vector<std::string> SoundMidi;
    if (engine.IsMidiFontLoaded())
    {
        TOOL::FilePath("./Resource/Music", &SoundMidi, "mid", "nullptr", nullptr);

        for (size_t i = 0; i < SoundMidi.size(); ++i)
        {
            std::string filePath = "./Resource/Music/" + SoundMidi[i] + ".mid";
            auto* asset = bank.LoadMidi(SoundMidi[i], filePath, engine.GetMidiFont());
            if (!asset)
            {
                LOGE("[SpatialAudio] Failed to load MIDI: %s", SoundMidi[i].c_str());
            }
        }
    }

    LOGD("[SpatialAudio] LoadAllResources complete — %zu assets loaded", bank.GetAssetCount());
}

} // namespace GAME::Audio