#include "SoundEffect.h"
#include "../Tool/Tool.h"
#include <iostream>


namespace GAME::SoundEffect {

	SoundEffect* SoundEffect::mSoundEffect = nullptr;

	SoundEffect::SoundEffect()
	{
		mSoloud.init();
		MidiFont.load(TimGM6mb_sf2);
		//gLPFilter.setParams(SoLoud::BiquadResonantFilter::LOWPASS, 10000, 0);//开启音效
		//gWave.setFilter(0, &gLPFilter);//绑定音效

		std::vector<std::string> SoundMp3;
		TOOL::FilePath("./Resources", &SoundMp3, "mp3", "nullptr", nullptr);

		mWaveS = new SoLoud::Wav * [SoundMp3.size()];
		for (size_t i = 0; i < SoundMp3.size(); i++)
		{
			mWaveS[i] = new SoLoud::Wav;
			mWaveS[i]->load(("./Resources/" + SoundMp3[i] + ".mp3").c_str());
			//std::cout << SoundMp3[i] << std::endl;
			SoundLibaryMap.insert(std::make_pair(SoundMp3[i], mWaveS[i]));
		}

		std::vector<std::string> SoundMidi;
		TOOL::FilePath("./Resources", &SoundMidi, "mid", "nullptr", nullptr);

		mMidiS = new SoLoud::Midi * [SoundMidi.size()];
		for (size_t i = 0; i < SoundMidi.size(); i++)
		{
			mMidiS[i] = new SoLoud::Midi;
			mMidiS[i]->load(("./Resources/" + SoundMidi[i] + ".mid").c_str(), MidiFont);
			//std::cout << SoundMidi[i] << std::endl;
			SoundLibaryMap.insert(std::make_pair(SoundMidi[i], mMidiS[i]));
		}

		SoundEffectsID = new ContinuousData<SoundStruct>(100);
	}

	SoundEffect::~SoundEffect()
	{
		delete[] mWaveS;
		delete[] mMidiS;
		delete SoundEffectsID;
		mSoloud.deinit();
	}

	void SoundEffect::Play(std::string name, SoundType Type, bool Loop, float Volume, float LeftAndRightVocalChannels)
	{
		if (SoundLibaryMap.find(name) == SoundLibaryMap.end()) {
			std::cout << "[SoundLibary] Error : Map Empty!" << std::endl;
			return;
		}
		switch (Type)
		{
			//																		音量   左右
		case MP3:
			if (Loop) {
				((SoLoud::Wav*)SoundLibaryMap[name])->setLooping(true);
			}
			SoundEffectsID->add({ mSoloud.play(*((SoLoud::Wav*)SoundLibaryMap[name]), Volume, LeftAndRightVocalChannels, 0, 0) });//播放这个音频
			break;
		case MIDI:
			if (Loop) {
				((SoLoud::Midi*)SoundLibaryMap[name])->setLooping(true);
			}
			SoundEffectsID->add({ mSoloud.play(*((SoLoud::Midi*)SoundLibaryMap[name]), Volume, LeftAndRightVocalChannels, 0, 0) });//播放这个音频
			break;
		default:
			break;
		}
	}

	void SoundEffect::Pause(SoLoud::handle Handle)
	{
		if (Handle == 0) {
			mSoloud.setPauseAll(1);//暂停所有音频
		}
		else {
			mSoloud.setPause(Handle, 1);//暂停这个音频
		}
	}

	void SoundEffect::SoundEffectEvent() {
		unsigned int SoundSize = SoundEffectsID->GetNumber();
		if (SoundSize != 0) {
			SoundStruct* SoundStructS = SoundEffectsID->Data();
			for (size_t i = 0; i < SoundSize; i++)
			{
				if (!mSoloud.isValidVoiceHandle(SoundStructS[i].Handle))
				{
					//std::cout << "Delete Sound :" << SoundStructS[i].Handle << std::endl;
					mSoloud.setPause(SoundStructS[i].Handle, 1);//暂停这个音频
					SoundEffectsID->Delete(i);
				}
			}

			//soloud.setVolume(kao, 1.0f);//设置音量

			//soloud.fadeFilterParameter(kao, 0, SoLoud::BiquadResonantFilter::FREQUENCY, 500, 1.0f);	//设置 频率
			//soloud.fadeFilterParameter(kao, 0, SoLoud::BiquadResonantFilter::BANDPASS, 1, 1.0f);		//设置 带通
			//soloud.fadeFilterParameter(kao, 0, SoLoud::BiquadResonantFilter::HIGHPASS, 1, 1.0f);		//设置 高通滤波器
			//soloud.fadeFilterParameter(kao, 0, SoLoud::BiquadResonantFilter::LOWPASS, 1, 1.0f);		//设置 低通滤波器
			//soloud.fadeFilterParameter(kao, 0, SoLoud::BiquadResonantFilter::BOOL_PARAM, 1, 1.0f);	//设置 BOOL参数
			//soloud.fadeFilterParameter(kao, 0, SoLoud::BiquadResonantFilter::FLOAT_PARAM, 1, 1.0f);	//设置 FLOAT参数
			//soloud.fadeFilterParameter(kao, 0, SoLoud::BiquadResonantFilter::INT_PARAM, 1, 1.0f);		//设置 INT参数
			//soloud.fadeFilterParameter(kao, 0, SoLoud::BiquadResonantFilter::RESONANCE, 1, 1.0f);		//设置 共振
			//soloud.fadeFilterParameter(kao, 0, SoLoud::BiquadResonantFilter::TYPE, 1, 1.0f);			//设置 类型
			//soloud.fadeFilterParameter(kao, 0, SoLoud::BiquadResonantFilter::WET, 1, 1.0f);			//设置 潮湿的
		}
	}

	void SoundEffect::SetVolume(float Volume) {
		unsigned int SoundSize = SoundEffectsID->GetNumber();
		if (SoundSize != 0) {
			SoundStruct* SoundStructS = SoundEffectsID->Data();
			for (size_t i = 0; i < SoundSize; i++)
			{
				mSoloud.setVolume(SoundStructS[i].Handle, Volume);//设置音量
			}
		}
	}
}