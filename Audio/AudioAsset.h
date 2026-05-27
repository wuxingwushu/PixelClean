// 音频资源管理层。用类型安全的 AudioAsset 替代旧的 void* 映射表。
// 支持三种资源类型：Sample(MP3/WAV)、Midi、SFXR(程序化音效)。
// AudioBank 通过 unordered_map 管理资源生命周期，析构时自动释放。
#pragma once
#include "soloud.h"
#include "soloud_wav.h"
#include "soloud_midi.h"
#include "soloud_sfxr.h"
#include <string>
#include <unordered_map>
#include <memory>

namespace GAME::Audio {

enum class AssetType
{
    Sample, // WAV/MP3 采样
    Midi,   // MIDI 音乐
    SFXR,   // 程序化音效（8-bit 复古风格）
};

// 单个音频资源的描述。根据 type 选择对应的 SoLoud 对象指针。
struct AudioAsset
{
    std::string    name;
    AssetType      type = AssetType::Sample;
    SoLoud::Wav*   wav = nullptr;
    SoLoud::Midi*  midi = nullptr;
    SoLoud::Sfxr*  sfxr = nullptr;
    float          duration = 0.0f; // 采样时长（秒），MIDI/SFXR 无意义
    bool           loaded = false;
};

// 音频资源库。负责加载、查询和释放所有音频资源。
// 使用 unique_ptr 管理 AudioAsset，确保异常安全。
class AudioBank
{
public:
    AudioBank();
    ~AudioBank();

    // 从文件加载 WAV/MP3 采样，失败返回 nullptr
    AudioAsset* LoadSample(const std::string& name, const std::string& filePath);
    // 从文件加载 MIDI，需要传入 MIDI 音色库
    AudioAsset* LoadMidi(const std::string& name, const std::string& filePath, SoLoud::SoundFont& sf);
    // 注册外部创建的 SFXR 对象（所有权转移给 AudioBank）
    AudioAsset* RegisterSFXR(const std::string& name, SoLoud::Sfxr* sfxr);

    // 按名称查找资源，不存在返回 nullptr
    AudioAsset* Get(const std::string& name);
    // 卸载指定资源
    void Unload(const std::string& name);
    // 卸载全部资源（析构时自动调用）
    void UnloadAll();
    size_t GetMemoryUsage() const;
    size_t GetAssetCount() const;

private:
    std::unordered_map<std::string, std::unique_ptr<AudioAsset>> mAssets;
};

} // namespace GAME::Audio