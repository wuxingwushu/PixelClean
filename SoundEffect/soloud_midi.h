#ifndef SOLOUD_TSF_H
#define SOLOUD_TSF_H

/*
播放有问题可以去跳一下 int MidiInstance::tick(float* stream, int SampleCount) 里面的：
for (SampleBlock = 1024; SampleCount; SampleCount -= SampleBlock, stream += SampleBlock)
SampleBlock 的 参数；但是要是2倍数的调整
*/
//midi 但是有问题 Debug没声音
/*
原因是 MidiInstance 在初始化时没有给 mMsec 赋值
导致在播放时 mMsec 为负值，导致无法正常播放。
*/

#include "soloud.h"

namespace SoLoud
{
	class Midi;
	class File;

	class MidiInstance : public AudioSourceInstance
	{
		Midi* mParent;
		void* mTrack;
		double mMsec = 0.0f;//这个值在播放器前要初始化为0;要不然没法正常播放。
		int mPlaying;
	
		int tick(float* stream, int SampleCount);
	public:
		MidiInstance(Midi* aParent);
		virtual ~MidiInstance();
		virtual unsigned int getAudio(float* aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize);
		virtual void seek(float aSeconds, float* mScratch, int mScratchSize);
		virtual unsigned int rewind();
		virtual bool hasEnded();
	};

	class SoundFont
	{
		friend class MidiInstance;
	protected:
		void* mHandle;
		char* mData;
		unsigned int mDataLen;
	public:
		SoundFont();
		virtual ~SoundFont();
		result load(const char* aFilename);
		result loadMem(unsigned char* aMem, unsigned int aLength, bool aCopy = false, bool aTakeOwnership = true);
		result loadFile(File* aFile);
	};

	class Midi : public AudioSource
	{
		friend class SoundFont;
		friend class MidiInstance;
	protected:
		SoundFont* mSoundFont;
		void* mHandle;
		char* mData;
		unsigned int mDataLen;
	public:
		Midi();
		virtual ~Midi();
		result load(const char* aFilename, SoundFont& sf);
		result loadMem(unsigned char* aMem, unsigned int aLength, SoundFont& sf, bool aCopy = false, bool aTakeOwnership = true);
		result loadFile(File* aFile, SoundFont& sf);
		virtual AudioSourceInstance* createInstance();
	};
};

#endif