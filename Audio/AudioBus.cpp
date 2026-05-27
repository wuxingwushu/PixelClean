// 混音总线系统实现。
// 默认音量：Environment=0.85, Music=0.6, SFX=0.8, Voice=0.7, UI=1.0, Master=1.0
// 淡入淡出使用指数衰减逼近：vol += (target - vol) * (dt / (remaining + dt))
// 总线语音设置 protected 防止 stopAll 误杀。
#include "AudioBus.h"
#include "../DebugLog.h"
#include <algorithm>

namespace GAME::Audio {

bool BusSystem::Initialize(SoLoud::Soloud* soloud)
{
    mSoloud = soloud;

    // 六条总线的默认音量
    const float defaultVolumes[] = {
        0.85f, // Environment
        0.6f,  // Music
        0.8f,  // SFX
        0.7f,  // Voice
        1.0f,  // UI
        1.0f   // Master
    };

    for (int i = 0; i < static_cast<int>(BusType::Count); ++i)
    {
        mVolumes[i] = defaultVolumes[i];
        mVolumeTargets[i] = defaultVolumes[i];
        mVolumeFadeTime[i] = 0.0f;
        mVolumeFadeElapsed[i] = 0.0f;
        mMuted[i] = false;

        // 在 SoLoud 引擎中创建总线语音
        mBusHandles[i] = mSoloud->play(mBuses[i]);
        if (mBusHandles[i] == 0)
        {
            LOGE("[BusSystem] Failed to create bus %d", i);
            return false;
        }
        mSoloud->setVolume(mBusHandles[i], defaultVolumes[i]);
    }

    // 保护总线语音不被 stopAll() 和声部偷取销毁
    for (int i = 0; i < static_cast<int>(BusType::Count); ++i)
    {
        mSoloud->setProtectVoice(mBusHandles[i], true);
    }

    LOGD("[BusSystem] All %d buses created", static_cast<int>(BusType::Count));
    return true;
}

void BusSystem::Shutdown()
{
    // 取消保护，允许后续 stopAll 销毁总线
    for (int i = 0; i < static_cast<int>(BusType::Count); ++i)
    {
        if (mBusHandles[i] != 0)
        {
            mSoloud->setProtectVoice(mBusHandles[i], false);
        }
    }
}

void BusSystem::SetVolume(BusType bus, float volume)
{
    int idx = static_cast<int>(bus);
    volume = std::max(0.0f, volume);  // 禁止负数
    mVolumes[idx] = volume;
    mVolumeTargets[idx] = volume;
    mVolumeFadeTime[idx] = 0.0f;      // 直接设置取消正在进行的淡入淡出
    if (!mMuted[idx])
    {
        mSoloud->setVolume(mBusHandles[idx], volume);
    }
}

float BusSystem::GetVolume(BusType bus) const
{
    return mVolumes[static_cast<int>(bus)];
}

void BusSystem::FadeVolume(BusType bus, float target, float durationSeconds)
{
    int idx = static_cast<int>(bus);
    if (durationSeconds <= 0.0f)
    {
        // 时长 ≤ 0 直接跳变
        SetVolume(bus, target);
    }
    else
    {
        mVolumeTargets[idx] = target;
        mVolumeFadeTime[idx] = durationSeconds;
        mVolumeFadeElapsed[idx] = 0.0f;
    }
}

void BusSystem::SetFilter(BusType bus, int slot, SoLoud::Filter* filter)
{
    int idx = static_cast<int>(bus);
    mBuses[idx].setFilter(slot, filter);
}

SoLoud::handle BusSystem::Play(BusType bus, SoLoud::AudioSource& source,
                                float volume, float pan)
{
    // 通过总线的 play() 方法播放，声音自动混入该总线
    return mBuses[static_cast<int>(bus)].play(source, volume, pan);
}

void BusSystem::SetPaused(BusType bus, bool paused)
{
    mSoloud->setPause(mBusHandles[static_cast<int>(bus)], paused);
}

void BusSystem::SetMuted(BusType bus, bool muted)
{
    int idx = static_cast<int>(bus);
    mMuted[idx] = muted;
    if (muted)
    {
        // 静音：强制设为 0
        mSoloud->setVolume(mBusHandles[idx], 0.0f);
    }
    else
    {
        // 取消静音：恢复之前的音量
        mSoloud->setVolume(mBusHandles[idx], mVolumes[idx]);
    }
}

SoLoud::handle BusSystem::GetBusHandle(BusType bus) const
{
    return mBusHandles[static_cast<int>(bus)];
}

void BusSystem::Update(float deltaTime)
{
    for (int i = 0; i < static_cast<int>(BusType::Count); ++i)
    {
        if (mVolumeFadeTime[i] > 0.0f)
        {
            mVolumeFadeElapsed[i] += deltaTime;
            float t = mVolumeFadeElapsed[i] / mVolumeFadeTime[i];
            if (t >= 1.0f)
            {
                // 淡入淡出完成，直接设为目标值
                mVolumes[i] = mVolumeTargets[i];
                mVolumeFadeTime[i] = 0.0f;
            }
            else
            {
                // 指数衰减逼近：越接近目标变化越慢，产生平滑曲线
                float remaining = mVolumeFadeTime[i] - mVolumeFadeElapsed[i];
                mVolumes[i] = mVolumes[i] + (mVolumeTargets[i] - mVolumes[i]) * (deltaTime / (remaining + deltaTime));
            }
            if (!mMuted[i])
            {
                mSoloud->setVolume(mBusHandles[i], mVolumes[i]);
            }
        }
    }
}

} // namespace GAME::Audio