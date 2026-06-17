#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

class CameraTransform;

class IControlStrategy {
public:
	virtual ~IControlStrategy() = default;

	// 每帧更新（由 Camera::update 调用）
	virtual void update(float dt, CameraTransform& transform) = 0;

	// 输入事件（可选重写）
	virtual void onMouseMove(double xpos, double ypos) {}
	virtual void onScroll(float delta) {}
	virtual void onKeyMove(glm::vec2 direction, float speed) {}

	// 设置跟随目标（仅跟随类策略有效）
	virtual void setTarget(const glm::vec3& target) {}

	// 边界
	virtual void setBoundary(const glm::vec2& min, const glm::vec2& max) {}

	// 抖动
	virtual void shake(float intensity, float duration) {}

	// 标识
	virtual const char* name() const = 0;
};
