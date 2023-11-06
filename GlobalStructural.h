#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

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

struct VPMatrices {
	glm::mat4 mViewMatrix;
	glm::mat4 mProjectionMatrix;

	VPMatrices() {
		mViewMatrix = glm::mat4(1.0f);
		mProjectionMatrix = glm::mat4(1.0f);
	}
};

struct ObjectUniform {
	glm::mat4 mModelMatrix;
	glm::vec3 StrikeColour = { 1.0f, 0.0f, 0.0f };
	float StrikeState = 0;

	ObjectUniform() {
		mModelMatrix = glm::mat4(1.0f);
	}
};

//Gif 描述符
struct ObjectUniformGIF {
	glm::mat4 mModelMatrix;
	glm::vec3 StrikeColour = { 1.0f, 0.0f, 0.0f };
	float StrikeState = 0;
	float chuang = 0.0f;
	unsigned int zhen = 0;

	ObjectUniformGIF() {
		mModelMatrix = glm::mat4(1.0f);
	}
};