#include "FollowControl2D.h"
#include "CameraTransform.h"
#include <algorithm>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <cstdlib>

FollowControl2D::FollowControl2D(float damping) : m_damping(damping) {}

void FollowControl2D::setDamping(float damping) { m_damping = damping; }
float FollowControl2D::getDamping() const { return m_damping; }
void FollowControl2D::setZoom(float zoom) { m_zoom = std::clamp(zoom, m_zoomMin, m_zoomMax); m_zoomTouched = true; }
float FollowControl2D::getZoom() const { return m_zoom; }
void FollowControl2D::setZoomRange(float min, float max) { m_zoomMin = min; m_zoomMax = max; }

void FollowControl2D::setTarget(const glm::vec3& target) {
	m_target = target;
	m_targetSet = true;
}

void FollowControl2D::setBoundary(const glm::vec2& min, const glm::vec2& max) {
	m_boundMin = min;
	m_boundMax = max;
}

void FollowControl2D::shake(float intensity, float duration) {
	m_shakeIntensity = intensity;
	m_shakeDuration = duration;
	m_shakeTimer = duration;
}

void FollowControl2D::onScroll(float delta) {
	// 滚动缩放：delta > 0 = 滚轮向前 = 放大（zoom 增大）
	m_scrollAccum += delta;
	// 应用 zoom 变化
	float z = m_zoom;
	if (z <= 10.0f) {
		z += (z / 2.0f) * delta;
	} else {
		z += delta * 5.0f;
	}
	setZoom(z);
}

void FollowControl2D::update(float dt, CameraTransform& transform) {
	// 1. 平滑跟随（仅在 setTarget 调用后才生效）
	glm::vec3 currentPos = transform.getPosition();
	glm::vec3 targetPos = m_target;

	if (m_targetSet && m_damping > 0.0f) {
		// 指数平滑
		float k = 1.0f - std::exp(-m_damping * dt);
		currentPos.x += (targetPos.x - currentPos.x) * k;
		currentPos.y += (targetPos.y - currentPos.y) * k;
	} else if (m_targetSet) {
		currentPos.x = targetPos.x;
		currentPos.y = targetPos.y;
	}
	// m_targetSet=false 时直接保留 currentPos.xy（外部已通过 setCameraPos 设好）

	// 2. 应用边界约束
	currentPos.x = std::clamp(currentPos.x, m_boundMin.x, m_boundMax.x);
	currentPos.y = std::clamp(currentPos.y, m_boundMin.y, m_boundMax.y);

	// 3. 应用 zoom → z 高度转换（仅在主动调用 setZoom/onScroll 后才生效）
	//    旧代码 m_position.z 直接修改的兼容：m_zoomTouched=false 时保留外部设置的 z
	if (m_zoomTouched) {
		currentPos.z = m_baseZoom / m_zoom;
	}

	// 4. 应用抖动偏移
	if (m_shakeTimer > 0.0f) {
		m_shakeTimer -= dt;
		float intensity = m_shakeIntensity * (m_shakeTimer / m_shakeDuration); // 衰减
		m_shakeOffset.x = (std::rand() % 2000 - 1000) / 1000.0f * intensity;
		m_shakeOffset.y = (std::rand() % 2000 - 1000) / 1000.0f * intensity;
		currentPos.x += m_shakeOffset.x;
		currentPos.y += m_shakeOffset.y;
	} else {
		m_shakeOffset = {0, 0, 0};
	}

	transform.setPosition(currentPos);
}
