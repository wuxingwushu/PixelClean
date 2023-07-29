#include "SoundEffect.h"



namespace GAME::SoundEffect {

	SoundEffect::SoundEffect()
	{
		// initialize SoLoud.
		soloud.init();
		MidiFont.load(TimGM6mb_sf2);
		//soloud.setGlobalVolume(1.0);
		//soloud.setPostClipScaler(10);
	}

	SoundEffect::~SoundEffect()
	{
		// Clean up SoLoud
		soloud.deinit();
	}

	void SoundEffect::Play()
	{
		if (soloud.getActiveVoiceCount() > 0) {//是否在播放
			soloud.setVolume(kao, 1.0f);//设置音量


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
		else {
			//                       音量   左右   
			kao = soloud.play(MidigWave, 1.0f,  0.0f, 0, 0);//播放这个音频
		}
	}

	void SoundEffect::Pause()
	{
		soloud.setPause(kao, 1);//暂停这个音频
		//soloud.setPauseAll(1);//暂停所有音频
	}

	void SoundEffect::PlayFile(char* path)
	{
		
		MidigWave.load(path, MidiFont); //读取音频
		//soloud.play(MidigWave, 1.0f);
		MidigWave.setLooping(1);//这个音频开启循环播放

		//gWave.load(path);
		//gWave.setLooping(1);//这个音频开启循环播放

		//gLPFilter.setParams(SoLoud::BiquadResonantFilter::LOWPASS, 10000, 0);//开启音效
		//gWave.setFilter(0, &gLPFilter);//绑定音效
		
	}

}