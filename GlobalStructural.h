#pragma once

struct PixelState {
	int X;
	int Y;
	bool State;
};

//子弹同步数据结构
struct SynchronizeBullet {
	float X;
	float Y;
	float angle;
	unsigned int Type;//那种子弹
};

struct ChatStrStruct {
	char* str = nullptr;
	unsigned int size = 0;

	~ChatStrStruct() {
		if (str != nullptr) {
			delete str;
		}
	}
};
