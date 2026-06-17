#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

class IProjectionStrategy {
public:
	virtual ~IProjectionStrategy() = default;

	// 计算投影矩阵（每帧或参数变化时调用）
	virtual glm::mat4 computeProjection(float aspectRatio) const = 0;

	// 缩放
	virtual void setZoom(float zoom) = 0;
	virtual float getZoom() const = 0;

	// 缩放宽高范围
	virtual void setZoomRange(float min, float max) = 0;

	// 标识
	virtual const char* name() const = 0;
};
