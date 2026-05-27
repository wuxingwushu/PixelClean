// 声部管理系统。在达到最大声部数时，根据优先级策略决定是否偷取/挤占已有声部。
// 支持 4 种偷取策略：None（不偷取）、Oldest（最老优先）、Quietest（最轻优先）、LowestPriority（最低优先级优先）。
// 每个声部追踪：SoLoud handle、源资源、所在总线、播放时刻、3D 标志等。
#pragma once
#include "soloud.h"
#include "AudioAsset.h"
#include "AudioBus.h"
#include <cstdint>
#include <vector>

namespace GAME::Audio {

enum class SoundPriority : uint8_t
{
    Critical = 0, // 关键音效（不会被偷取）
    High     = 1, // 高优先级
    Medium   = 2, // 中等优先级
    Low      = 3, // 低优先级
};

enum class VoiceStealPolicy : uint8_t
{
    None           = 0, // 不允许偷取，达到上限后丢弃新请求
    Oldest         = 1, // 偷取最老的声部
    Quietest       = 2, // 偷取音量最低的声部
    LowestPriority = 3, // 偷取优先级最低的声部
};

struct VoiceInfo
{
    SoLoud::handle handle = 0;
    AudioAsset*   source = nullptr;
    BusType       bus = BusType::SFX;
    SoundPriority priority = SoundPriority::Medium;
    float         currentVolume = 0.0f;
    bool          is3D = false;
    bool          alive = true;
    double        spawnTime = 0.0;   // 分配时刻（模拟时间）
};

class VoiceManager
{
public:
    VoiceManager() = default;

    // 设置最大声部数并预分配内存
    void Initialize(int maxVoices = 128);
    // 设置声部偷取策略
    void SetPolicy(VoiceStealPolicy policy);

    // 请求分配一个声部。如果未达上限，直接复用或新增；否则按策略偷取。
    // 返回值可能为 nullptr（偷取失败或不支持偷取）。
    VoiceInfo* Allocate(SoundPriority priority);
    // 释放指定句柄的声部
    void Free(SoLoud::handle handle);
    // 每帧时间步进
    void Update();

    int GetActiveCount() const;
    int GetVirtualCount() const;
    int GetMaxVoices() const { return mMaxVoices; }

private:
    // 按当前策略找到最合适的可偷取声部并标记为死亡
    bool TryStealVoice(SoundPriority newPriority);

    int mMaxVoices = 128;
    VoiceStealPolicy mPolicy = VoiceStealPolicy::LowestPriority;
    std::vector<VoiceInfo> mVoices;
    double mTime = 0.0;   // 内部模拟时间，每帧递进 1/60 秒
};

} // namespace GAME::Audio