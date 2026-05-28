#include "SoundEffect.h"
#include "../Tool/Tool.h"
#include "../DebugLog.h"
#include "AudioBus.h"
#include <iostream>
#include <vector>

namespace GAME::SoundEffect {

	SoundEffect* SoundEffect::mSoundEffect = nullptr;

	SoundEffect::SoundEffect()
	{
		LOGD("[SoundEffectCompat] Constructor — initializing new AudioEngine");

		auto& engine = GAME::Audio::AudioEngine::Get();
		if (!engine.Initialize())
		{
			LOGE("[SoundEffectCompat] AudioEngine initialization failed");
			return;
		}

		auto& bank = engine.GetBank();

		std::vector<std::string> SoundMp3;
		TOOL::FilePath("./Resources", &SoundMp3, "mp3", "nullptr", nullptr);

		for (size_t i = 0; i < SoundMp3.size(); ++i)
		{
			std::string filePath = "./Resources/" + SoundMp3[i] + ".mp3";
			auto* asset = bank.LoadSample(SoundMp3[i], filePath);
			if (!asset)
			{
				LOGE("[SoundEffectCompat] Failed to load MP3: %s", SoundMp3[i].c_str());
			}
		}

		std::vector<std::string> SoundMidi;
		TOOL::FilePath("./Resources", &SoundMidi, "mid", "nullptr", nullptr);

		for (size_t i = 0; i < SoundMidi.size(); ++i)
		{
			std::string filePath = "./Resources/" + SoundMidi[i] + ".mid";
			auto* asset = bank.LoadMidi(SoundMidi[i], filePath, engine.GetMidiFont());
			if (!asset)
			{
				LOGE("[SoundEffectCompat] Failed to load MIDI: %s", SoundMidi[i].c_str());
			}
		}

		LOGD("[SoundEffectCompat] Construction complete — %zu assets loaded", bank.GetAssetCount());
	}

	SoundEffect::~SoundEffect()
	{
	}

	void SoundEffect::Destroy()
	{
		if (mSoundEffect)
		{
			GAME::Audio::AudioEngine::Get().Shutdown();
			delete mSoundEffect;
			mSoundEffect = nullptr;
		}
	}

	void SoundEffect::Play(std::string name, SoundType Type, bool Loop, float Volume, float LeftAndRightVocalChannels)
	{
		auto& engine = GAME::Audio::AudioEngine::Get();
		auto& bank = engine.GetBank();

		auto* asset = bank.Get(name);
		if (!asset)
		{
			std::cout << "[SoundLibary] Error : Map Empty! key=" << name << std::endl;
			return;
		}

		if (Type == MP3)
		{
			if (Loop && asset->wav)
			{
				asset->wav->setLooping(true);
			}
			engine.GetSoloud().play(*asset->wav, Volume, LeftAndRightVocalChannels);
			if (Loop && asset->wav)
			{
				asset->wav->setLooping(false);
			}
		}
		else if (Type == MIDI)
		{
			if (Loop && asset->midi)
			{
				asset->midi->setLooping(true);
			}
			engine.GetSoloud().play(*asset->midi, Volume, LeftAndRightVocalChannels);
			if (Loop && asset->midi)
			{
				asset->midi->setLooping(false);
			}
		}
	}

	void SoundEffect::Pause(SoLoud::handle Handle)
	{
		auto& soloud = GAME::Audio::AudioEngine::Get().GetSoloud();
		if (Handle == 0) {
			soloud.setPauseAll(1);
		}
		else {
			soloud.setPause(Handle, 1);
		}
	}

	void SoundEffect::SoundEffectEvent() {
		GAME::Audio::AudioEngine::Get().Update(TOOL::FPStime);
	}

	void SoundEffect::SetVolume(float Volume) {
		GAME::Audio::AudioEngine::Get().GetBusSystem().SetVolume(GAME::Audio::BusType::Master, Volume);
	}
}