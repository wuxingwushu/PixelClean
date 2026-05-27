// 混音总线系统。提供 6 条独立总线用于分类混音。
// 每条总线对应一个 SoLoud::Bus 实例，可设置独立音量、静音、淡入淡出和滤波器。
// 总线在引擎初始化时创建并设为 protected，不会因 stopAll() 而被销毁。
// 典型路由：SFX→Master、Music→Master、Environment→Master（带混响/回声）
#pragma once
#include "soloud.h"
#include "soloud_bus.h"
#include <cstdint>

namespace GAME::Audio {

enum class BusType : uint8_t
{
    Environment = 0, // 环境总线：通常挂载混响/回声滤波器
    Music       = 1, // 音乐总线
    SFX         = 2, // 音效总线（枪声、脚步声等）
    Voice       = 3, // 语音总线（对话、旁白）
    UI          = 4, // UI 总线（按钮点击等 2D 音效）
    Master      = 5, // 主控总线：控制总输出
    Count       = 6  // 总线数量（用于数组大小）
};

class BusSystem
{
public:
    BusSystem() = default;
    ~BusSystem() = default;

    // 创建全部 6 条总线并设置默认音量。成功返回 true。
    bool Initialize(SoLoud::Soloud* soloud);
    // 取消保护总线语音（允许被 stopAll 销毁）
    void Shutdown();

    // 直接设置总线音量（0.0 ~ +∞）
    void SetVolume(BusType bus, float volume);
    // 获取当前音量
    float GetVolume(BusType bus) const;
    // 平滑过渡音量到目标值，durationSeconds 秒内完成
    void FadeVolume(BusType bus, float target, float durationSeconds);

    // 在 bus 的 slot 位设置滤波器（如混响、低通等）
    void SetFilter(BusType bus, int slot, SoLoud::Filter* filter);

    // 在指定总线上播放 AudioSource，返回 handle
    SoLoud::handle Play(BusType bus, SoLoud::AudioSource& source,
                        float volume = 1.0f, float pan = 0.0f);
    // 暂停/恢复整条总线
    void SetPaused(BusType bus, bool paused);
    // 静音/取消静音（静音时记录原始音量，取消时恢复）
    void SetMuted(BusType bus, bool muted);

    // 获取 SoLoud 总线句柄（供 play3d 等高级 API 使用）
    SoLoud::handle GetBusHandle(BusType bus) const;

    // 每帧调用，处理淡入淡出平滑过渡
    void Update(float deltaTime);

private:
    SoLoud::Soloud* mSoloud = nullptr;
    SoLoud::Bus mBuses[static_cast<int>(BusType::Count)];        // SoLoud 总线对象
    SoLoud::handle mBusHandles[static_cast<int>(BusType::Count)]; // 对应的语音句柄
    float mVolumes[static_cast<int>(BusType::Count)];             // 当前音量
    float mVolumeTargets[static_cast<int>(BusType::Count)];       // 淡入淡出目标音量
    float mVolumeFadeTime[static_cast<int>(BusType::Count)];      // 淡入淡出总时长
    float mVolumeFadeElapsed[static_cast<int>(BusType::Count)];   // 淡入淡出已过时间
    bool mMuted[static_cast<int>(BusType::Count)];                // 静音标志
};

} // namespace GAME::Audio