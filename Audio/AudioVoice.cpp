// 声部管理器实现。
// Allocate 分配策略：优先复用已死亡的声部 → 新增（未达上限）→ 按策略偷取。
// TryStealVoice 根据 VoiceStealPolicy 计算 "最佳受害者" 分数，越高越容易被偷：
//   Oldest: score = -spawnTime（越早创建分数越高）
//   Quietest: score = 1 - currentVolume（音量越低分数越高）
//   LowestPriority: score = 4 - priority（优先级数值越低分数越高）
// Critical 优先级永远不会被偷取。
#include "AudioVoice.h"
#include "../DebugLog.h"

namespace GAME::Audio {

void VoiceManager::Initialize(int maxVoices)
{
    mMaxVoices = maxVoices;
    mVoices.reserve(mMaxVoices);
    mVoices.clear();
    LOGD("[VoiceManager] Initialized with max %d voices", mMaxVoices);
}

void VoiceManager::SetPolicy(VoiceStealPolicy policy)
{
    mPolicy = policy;
}

VoiceInfo* VoiceManager::Allocate(SoundPriority priority)
{
    // 策略 1：查找已死亡可复用的声部
    for (auto& v : mVoices)
    {
        if (!v.alive)
        {
            v.alive = true;
            v.priority = priority;
            v.spawnTime = mTime;
            v.handle = 0;
            v.currentVolume = 1.0f;
            return &v;
        }
    }

    // 策略 2：未达上限，新增一个声部
    if ((int)mVoices.size() < mMaxVoices)
    {
        mVoices.push_back({});
        VoiceInfo& v = mVoices.back();
        v.alive = true;
        v.priority = priority;
        v.spawnTime = mTime;
        v.handle = 0;
        v.currentVolume = 1.0f;
        return &v;
    }

    // 策略 3：已达上限，尝试偷取
    if (TryStealVoice(priority))
    {
        // 偷取成功后会有一个声部被标记为 !alive，再次搜索复用
        for (auto& v : mVoices)
        {
            if (!v.alive && v.priority != SoundPriority::Critical)
            {
                v.alive = true;
                v.priority = priority;
                v.spawnTime = mTime;
                v.handle = 0;
                v.currentVolume = 1.0f;
                return &v;
            }
        }
    }

    return nullptr;
}

void VoiceManager::Free(SoLoud::handle handle)
{
    for (auto& v : mVoices)
    {
        if (v.handle == handle)
        {
            v.alive = false;
            v.handle = 0;
            return;
        }
    }
}

void VoiceManager::Update()
{
    // 模拟时间以 60fps 步进
    mTime += 1.0 / 60.0;
}

int VoiceManager::GetActiveCount() const
{
    int count = 0;
    for (auto& v : mVoices)
    {
        if (v.alive) ++count;
    }
    return count;
}

int VoiceManager::GetVirtualCount() const
{
    return (int)mVoices.size();
}

bool VoiceManager::TryStealVoice(SoundPriority newPriority)
{
    if (mPolicy == VoiceStealPolicy::None)
        return false;

    VoiceInfo* victim = nullptr;
    float bestScore = -1.0f;

    for (auto& v : mVoices)
    {
        // Critical 优先级不可偷取
        if (!v.alive || v.priority == SoundPriority::Critical)
            continue;

        float score = 0.0f;
        switch (mPolicy)
        {
            case VoiceStealPolicy::Oldest:
                // 负数 spawnTime 意味着越老分数越高（因为 -oldest > -newest）
                score = static_cast<float>(-v.spawnTime);
                break;
            case VoiceStealPolicy::Quietest:
                // 音量越低越容易被偷
                score = 1.0f - v.currentVolume;
                break;
            case VoiceStealPolicy::LowestPriority:
                // 优先级越低（数值越大）分数越高
                score = static_cast<float>(4 - static_cast<int>(v.priority));
                break;
            default:
                break;
        }

        if (score > bestScore)
        {
            bestScore = score;
            victim = &v;
        }
    }

    if (victim)
    {
        victim->alive = false;
        return true;
    }
    return false;
}

} // namespace GAME::Audio