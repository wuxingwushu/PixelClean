#pragma once
#include "zlib/zlib.h"
#include "../Character/Crowd.h"
#include "../Arms/Arms.h"
#include "../Labyrinth/Labyrinth.h"

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


typedef void (*_SynchronizeCallback)(bufferevent* be, void* Data);//函数结构
//数据
struct SynchronizeData {
	void* Pointer;//数据指针
	unsigned int Size;//数据数量
};

//标签事件结构体
struct _Synchronize
{
	void* mPointer = nullptr;//数据指针
	unsigned int Size;//单个数据大小
	_SynchronizeCallback mSynchronizeCallback = nullptr;//函数
};

//地图点事件数据结构
struct PixelSynchronize {
	int X;
	int Y;
	bool State;
};

struct BufferEventSingleData
{
	PileUp<SynchronizeBullet>* mSubmitBullet;//子弹一次性数据
	PileUp<PixelSynchronize>* mLabyrinthPixel;//地图点事件

	bool* mBrokenData;//破碎状态

	BufferEventSingleData(unsigned int size) {
		mSubmitBullet = new PileUp<SynchronizeBullet>(size);
		mLabyrinthPixel = new PileUp<PixelSynchronize>(size);
	}

	~BufferEventSingleData() {
		delete mSubmitBullet;
		delete mLabyrinthPixel;
	}
};

//玩家同步数据结构
struct PlayerPos {
	//位置
	float X;
	float Y;
	//角度
	float ang;
	//一次性同步数据
	BufferEventSingleData* mBufferEventSingleData = nullptr;
	//钥匙
	evutil_socket_t Key;
};

//玩家破碎状态
struct PlayerBroken {
	evutil_socket_t Key;	//玩家 Key
	bool Broken[16 * 16];	//破碎状态
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

	void SetGamePlayer(GAME::GamePlayer* GamePlayer) {
		mGamePlayer = GamePlayer;
	}

	void SetLabyrinth(GAME::Labyrinth* Labyrinth) {
		mLabyrinth = Labyrinth;
	}

	[[nodiscard]] constexpr GAME::Arms* GetArms() const noexcept {
		return mArms;
	}

	[[nodiscard]] constexpr GAME::Crowd* GetCrowd() const noexcept {
		return mCrowd;
	}

	[[nodiscard]] constexpr GAME::GamePlayer* GetGamePlayer() const noexcept {
		return mGamePlayer;
	}

	[[nodiscard]] constexpr GAME::Labyrinth* GetLabyrinth() const noexcept {
		return mLabyrinth;
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
	GAME::Arms* mArms = nullptr;//武器
	GAME::Crowd* mCrowd = nullptr;//玩家群
	GAME::GamePlayer* mGamePlayer = nullptr;//玩家
	GAME::Labyrinth* mLabyrinth = nullptr;//迷宫

	//标签事件储存入口数据
	std::map<unsigned int, _Synchronize> mSynchronizeMap = {};
};