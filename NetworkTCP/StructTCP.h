#pragma once
#include "zlib/zlib.h"
#include"../GlobalStructural.h"
#include "../Tool/PileUp.h"
#include "../Tool/QueueData.h"
#include "../Tool/LimitUse.h"
#include <event2/bufferevent.h>

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

struct BufferEventSingleData
{
	PileUp<SynchronizeBullet>* mSubmitBullet = nullptr;//子弹一次性数据
	PileUp<PixelState>* mLabyrinthPixel = nullptr;//地图点事件
	PileUp<PixelState>* mCharacterPixel = nullptr;//人物点事件
	QueueData<LimitUse<ChatStrStruct*>*>* mStr = nullptr;//对话

	bool* mBrokenData = nullptr;//破碎状态

	BufferEventSingleData(unsigned int size) {
		mSubmitBullet = new PileUp<SynchronizeBullet>(size);
		mLabyrinthPixel = new PileUp<PixelState>(size);
		mCharacterPixel = new PileUp<PixelState>(size);
		mStr = new QueueData<LimitUse<ChatStrStruct*>*>(size);
	}

	~BufferEventSingleData() {
		delete mSubmitBullet;
		delete mLabyrinthPixel;
		delete mCharacterPixel;
		delete mStr;
	}
};

//玩家同步数据结构
struct RoleSynchronizationData {
	//位置
	float X;
	float Y;
	//角度
	float ang;
	//一次性同步数据
	BufferEventSingleData* mBufferEventSingleData = nullptr;
	//钥匙
	evutil_socket_t Key;

	~RoleSynchronizationData() {//在 ContinuousMap 中两个值交换时 std::swap<> 会 调用 ~析构() ,导致数据丢失
		/*if (mBufferEventSingleData != nullptr) {
			delete mBufferEventSingleData;
			mBufferEventSingleData = nullptr;
		}*/
	}
};

//玩家破碎状态
struct PlayerBroken {
	evutil_socket_t Key;	//玩家 Key
	bool Broken[16 * 16];	//破碎状态
};


