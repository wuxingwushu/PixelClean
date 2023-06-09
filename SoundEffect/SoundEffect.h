#include "soloud.h"
#include "soloud_wav.h"
#include "soloud_midi.h"
#include "soloud_file.h"
#include "soloud_biquadresonantfilter.h"
#include "../FilePath.h"



namespace GAME::SoundEffect {

	class SoundEffect
	{
	public:
		SoundEffect();
		~SoundEffect();

		void Play();
		void Pause();

		

		void PlayFile(char* path);


	private:

		SoLoud::Soloud soloud;
		//SoLoud::Wav gWave;

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
		SoLoud::Midi MidigWave;
		SoLoud::SoundFont MidiFont;
		

		int kao;//音效ID

		SoLoud::BiquadResonantFilter gLPFilter;
	};

	

}