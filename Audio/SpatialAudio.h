// 空间音频系统。封装 SoLoud 3D API，提供 3D/2D 音源播放和遮挡模拟。
// Play3D 使用 SoLoud::play3d 根据声源位置/速度自动计算空间衰减和多普勒效应。
// SetSourceOcclusion 通过调节低通滤波器频率模拟遮挡效果（完全遮挡 → 400Hz）。
// 内部持有 VoiceManager 管理 3D 声部的生命周期和优先级偷取。
#pragma once
#include "soloud.h"
#include "AudioAsset.h"
#include "AudioBus.h"
#include "AudioVoice.h"
#include <string>

namespace GAME::Audio {

// 3D 声源配置
struct Source3DConfig
{
    float posX = 0, posY = 0, posZ = 0;   // 位置
    float velX = 0, velY = 0, velZ = 0;   // 速度（用于多普勒效应）
    float minDistance = 1.0f;              // 最小衰减距离
    float maxDistance = 50.0f;             // 最大衰减距离
    float rolloffFactor = 1.0f;            // 衰减曲线指数
    float dopplerFactor = 1.0f;            // 多普勒效应强度
};

// 3D 监听者配置（通常绑定到玩家摄像机）
struct Listener3DConfig
{
    float posX = 0, posY = 0, posZ = 0;    // 位置
    float velX = 0, velY = 0, velZ = 0;    // 速度
    float atX = 0, atY = 0, atZ = -1;      // 朝向（forward）
    float upX = 0, upY = 1, upZ = 0;       // 上方向
};

class SpatialAudioSystem
{
public:
    SpatialAudioSystem() = default;

    bool Initialize(SoLoud::Soloud* soloud, BusSystem* busSystem,
                    AudioBank* bank, int maxVoices = 128);
    void Shutdown();

    // 设置监听者完整参数（位置 + 朝向 + 速度）
    void SetListener(const Listener3DConfig& config);
    // 快速设置监听者位置（保持朝向不变）
    void SetListenerPosition(float x, float y, float z = 0);

    // 播放 3D 音效：自动应用空间衰减、多普勒和距离裁剪
    SoLoud::handle Play3D(const std::string& assetName,
                          const Source3DConfig& config,
                          SoundPriority priority = SoundPriority::Medium,
                          BusType bus = BusType::SFX,
                          float volume = 1.0f);

    // 播放 2D 音效（不受空间位置影响，常用于 UI）
    SoLoud::handle Play2D(const std::string& assetName,
                          float volume = 1.0f,
                          SoundPriority priority = SoundPriority::Critical,
                          BusType bus = BusType::UI);

    // 更新已播放 3D 声源的位置/速度（如移动角色）
    void UpdateSource3D(SoLoud::handle handle, const Source3DConfig& config);
    // 设置声源遮挡系数 0~1，模拟墙壁遮挡
    void SetSourceOcclusion(SoLoud::handle handle, float occlusion);

    // 直接播放 AudioSource 到指定总线（底层 API）
    SoLoud::handle PlayDirect(SoLoud::AudioSource& source,
                              float volume = 1.0f, float pan = 0.0f,
                              BusType bus = BusType::SFX);

    void Update(float deltaTime);

    VoiceManager& GetVoiceManager() { return mVoiceManager; }

private:
    SoLoud::Soloud* mSoloud = nullptr;
    BusSystem* mBusSystem = nullptr;
    AudioBank* mBank = nullptr;
    VoiceManager mVoiceManager;
    Listener3DConfig mListener;
};

} // namespace GAME::Audio