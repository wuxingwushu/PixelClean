// 音频资源库实现。
// LoadSample/LoadMidi 创建 SoLoud 对象并调用 load()，检查返回值。
// RegisterSFXR 直接接管外部创建的 SFXR 对象。
// UnloadAll 遍历释放 SoLoud 对象的堆内存（Wav/Midi delete，SFXR 由外部管理）。
#include "AudioAsset.h"
#include "../DebugLog.h"

namespace GAME::Audio {

AudioBank::AudioBank() = default;
AudioBank::~AudioBank() { UnloadAll(); }

AudioAsset* AudioBank::LoadSample(const std::string& name, const std::string& filePath)
{
    auto asset = std::make_unique<AudioAsset>();
    asset->name = name;
    asset->type = AssetType::Sample;
    asset->wav = new SoLoud::Wav;

    SoLoud::result res = asset->wav->load(filePath.c_str());
    if (res != SoLoud::SO_NO_ERROR)
    {
        LOGE("[AudioBank] Failed to load Sample: %s (error: %d)", filePath.c_str(), res);
        delete asset->wav;
        return nullptr;
    }

    asset->duration = (float)asset->wav->getLength();
    asset->loaded = true;

    AudioAsset* ptr = asset.get();
    mAssets[name] = std::move(asset);
    return ptr;
}

AudioAsset* AudioBank::LoadMidi(const std::string& name, const std::string& filePath, SoLoud::SoundFont& sf)
{
    auto asset = std::make_unique<AudioAsset>();
    asset->name = name;
    asset->type = AssetType::Midi;
    asset->midi = new SoLoud::Midi;

    SoLoud::result res = asset->midi->load(filePath.c_str(), sf);
    if (res != SoLoud::SO_NO_ERROR)
    {
        LOGE("[AudioBank] Failed to load MIDI: %s (error: %d)", filePath.c_str(), res);
        delete asset->midi;
        return nullptr;
    }

    asset->loaded = true;

    AudioAsset* ptr = asset.get();
    mAssets[name] = std::move(asset);
    return ptr;
}

AudioAsset* AudioBank::RegisterSFXR(const std::string& name, SoLoud::Sfxr* sfxr)
{
    auto asset = std::make_unique<AudioAsset>();
    asset->name = name;
    asset->type = AssetType::SFXR;
    asset->sfxr = sfxr;       // 接管外部创建的对象
    asset->loaded = true;

    AudioAsset* ptr = asset.get();
    mAssets[name] = std::move(asset);
    return ptr;
}

AudioAsset* AudioBank::Get(const std::string& name)
{
    auto it = mAssets.find(name);
    if (it != mAssets.end())
        return it->second.get();
    return nullptr;
}

void AudioBank::Unload(const std::string& name)
{
    mAssets.erase(name);
}

void AudioBank::UnloadAll()
{
    // 手动释放 SoLoud 对象（unique_ptr 不管理裸 SoLoud 指针）
    for (auto& pair : mAssets)
    {
        AudioAsset* asset = pair.second.get();
        if (asset->type == AssetType::Sample && asset->wav)
            delete asset->wav;
        else if (asset->type == AssetType::Midi && asset->midi)
            delete asset->midi;
        // SFXR 由外部释放，不在这里 delete
    }
    mAssets.clear();
}

size_t AudioBank::GetMemoryUsage() const
{
    return mAssets.size() * sizeof(AudioAsset);
}

size_t AudioBank::GetAssetCount() const
{
    return mAssets.size();
}

} // namespace GAME::Audio