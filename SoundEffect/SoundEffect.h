#pragma once
#include "soloud.h"
#include "soloud_wav.h"
#include "soloud_midi.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/AudioAsset.h"
#include <string>

struct SoundStruct
{
	SoLoud::handle Handle;
};

enum SoundType {
	MP3 = 0,
	MIDI
};

namespace GAME::SoundEffect {

	class SoundEffect
	{
	public:
		static SoundEffect* GetSoundEffect() {
			if (mSoundEffect == nullptr) {
				mSoundEffect = new SoundEffect();
			}
			return mSoundEffect;
		};
		~SoundEffect();

		void Destroy();

		void* GetWav(std::string name) {
			auto* asset = GAME::Audio::AudioEngine::Get().GetBank().Get(name);
			if (asset && asset->type == GAME::Audio::AssetType::Sample)
				return asset->wav;
			return nullptr;
		}

		void Play(std::string name, SoundType Type, bool Loop = false, float Volume = 1.0f, float LeftAndRightVocalChannels = 0.0f);

		void Pause(SoLoud::handle Handle);

		void SoundEffectEvent();

		void SetVolume(float Volume);

		SoLoud::Soloud* GetSoloud() {
			return &GAME::Audio::AudioEngine::Get().GetSoloud();
		}

		void StopAll() {
			GAME::Audio::AudioEngine::Get().GetSoloud().stopAll();
		}

	private:
		static SoundEffect* mSoundEffect;
		SoundEffect();
	};

}