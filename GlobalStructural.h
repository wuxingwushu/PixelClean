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

//字符串
struct ChatStrStruct {
	char* str = nullptr;
	unsigned int size = 0;

	~ChatStrStruct() {
		if (str != nullptr) {
			delete str;
		}
	}
};

//Gif 描述符
struct ObjectUniformGIF {
	glm::mat4 mModelMatrix;
	float chuang = 0.0f;
	unsigned int zhen = 0;

	ObjectUniformGIF() {
		mModelMatrix = glm::mat4(1.0f);
	}
};