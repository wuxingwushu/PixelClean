#pragma once
#include "soloud.h"
#include "soloud_wav.h"
#include "soloud_midi.h"
#include "soloud_file.h"
#include "soloud_biquadresonantfilter.h"
#include "../FilePath.h"
#include "../Tool/ContinuousData.h"
#include <map>
#include <string>

/*
播放有问题可以去跳一下 int MidiInstance::tick(float* stream, int SampleCount) 里面的：
for (SampleBlock = 512; SampleCount; SampleCount -= SampleBlock, stream += SampleBlock)
SampleBlock 的 参数；但是要是2倍数的调整
*/

//midi 但是有问题 Debug没声音
/*
原因是 MidiInstance 在初始化时没有给 mMsec 赋值
导致在播放时 mMsec 为负值，导致无法正常播放。
*/

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

		void* GetWav(std::string name) {
			return SoundLibaryMap[name];
		}

		void Play(std::string name, SoundType Type, bool Loop = false, float Volume = 1.0f, float LeftAndRightVocalChannels = 0.0f);

		void Pause(SoLoud::handle Handle);

		void SoundEffectEvent();

		void SetVolume(float Volume);

	private:
		static SoundEffect* mSoundEffect;
		SoundEffect();
		
		SoLoud::Soloud mSoloud;//主
		SoLoud::SoundFont MidiFont;//音色
		SoLoud::Wav** mWaveS;
		SoLoud::Midi** mMidiS;
		std::map<std::string, void*> SoundLibaryMap;
		
		ContinuousData<SoundStruct>* SoundEffectsID;//音效ID
		//SoLoud::BiquadResonantFilter gLPFilter;//音效滤波器
	};

	

}