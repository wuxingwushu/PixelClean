#pragma once
#include "IControlStrategy.h"

class FollowControl2D : public IControlStrategy {
public:
	FollowControl2D(float damping = 5.0f);

	// 设置跟随阻尼（0 = 瞬间跟随，越大越慢）
	void setDamping(float damping);
	float getDamping() const;

	// 设置缩放（替代旧 m_position.z 控制）
	void setZoom(float zoom);
	float getZoom() const;
	void setZoomRange(float min, float max);

	// IControlStrategy
	void update(float dt, CameraTransform& transform) override;
	void onScroll(float delta) override;
	void setTarget(const glm::vec3& target) override;
	void setBoundary(const glm::vec2& min, const glm::vec2& max) override;
	void shake(float intensity, float duration) override;
	const char* name() const override { return "Follow2D"; }

private:
	glm::vec3 m_target{0, 0, 0};
	float m_damping = 5.0f;
	bool m_targetSet = false;  // setTarget 是否被调用过（未调用时不跟随）

	// 缩放（替代旧 m_position.z）
	float m_zoom = 1.0f;
	float m_zoomMin = 0.1f;
	float m_zoomMax = 100.0f;
	float m_scrollAccum = 0.0f;

	// 边界
	glm::vec2 m_boundMin{-FLT_MAX, -FLT_MAX};
	glm::vec2 m_boundMax{FLT_MAX, FLT_MAX};

	// 抖动
	float m_shakeIntensity = 0.0f;
	float m_shakeDuration = 0.0f;
	float m_shakeTimer = 0.0f;
	glm::vec3 m_shakeOffset{0};

	// 基础 zoom 层（相机 Z 高度）
	float m_baseZoom = 400.0f;

	// 是否已主动设置过 zoom（避免覆盖旧代码直接改的 m_position.z）
	bool m_zoomTouched = false;
};
