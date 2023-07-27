#pragma once
#include "zlib/zlib.h"
#include "../Character/Crowd.h"
#include "../Arms/Arms.h"

struct Zip {
	z_stream* y;
	z_stream* j;

	Zip() {
		y = new z_stream();
		deflateInit(y, Z_DEFAULT_COMPRESSION);
		j = new z_stream();
		inflateInit(j);
	}

	~Zip() {
		delete y;
		delete j;
	}
};

struct DataHeader//数据头
{
	unsigned int Key;//标签钥匙
	unsigned int Size;//数据大小
};


typedef void (*_SynchronizeCallback)(bufferevent* be, void* Data);
struct SynchronizeData {
	void* Pointer;
	unsigned int Size;
};

struct _Synchronize
{
	void* mPointer = nullptr;
	_SynchronizeCallback mSynchronizeCallback = nullptr;
};


class SynchronizeClass
{
public:
	void SetArms(GAME::Arms* Arms) {
		mArms = Arms;
	}

	void SetCrowd(GAME::Crowd* Crowd) {
		mCrowd = Crowd;
	}

	[[nodiscard]] constexpr GAME::Arms* GetArms() const noexcept {
		return mArms;
	}

	[[nodiscard]] constexpr GAME::Crowd* GetCrowd() const noexcept {
		return mCrowd;
	}

	void AddSynchronizeMap(unsigned int I, _Synchronize synchronize) {
		mSynchronizeMap.insert(std::make_pair(I, synchronize));
	}

	_Synchronize GetSynchronizeMap(unsigned int I) {
		if (mSynchronizeMap.find(I) == mSynchronizeMap.end()) {
			return _Synchronize{ 0 };
		}
		return mSynchronizeMap[I];
	}

private:
	GAME::Arms* mArms = nullptr;
	GAME::Crowd* mCrowd = nullptr;

	//标签事件储存入口数据
	std::map<unsigned int, _Synchronize> mSynchronizeMap = {};
};