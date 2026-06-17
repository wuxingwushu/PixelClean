#include "FreeControl2D.h"
#include "CameraTransform.h"
#include <algorithm>
#include <cmath>

FreeControl2D::FreeControl2D(float moveSpeed) : m_moveSpeed(moveSpeed) {}

void FreeControl2D::setMoveSpeed(float speed) { m_moveSpeed = speed; }

void FreeControl2D::onKeyMove(glm::vec2 direction, float speed) {
	m_moveInput = direction;
	if (speed > 0) m_moveSpeed = speed;
}

void FreeControl2D::onScroll(float delta) {
	float z = m_zoom;
	if (z <= 10.0f) {
		z += (z / 2.0f) * delta;
	} else {
		z += delta * 5.0f;
	}
	m_zoom = std::clamp(z, m_zoomMin, m_zoomMax);
}

void FreeControl2D::update(float dt, CameraTransform& transform) {
	glm::vec3 pos = transform.getPosition();

	// 移动
	pos.x += m_moveInput.x * m_moveSpeed * dt;
	pos.y += m_moveInput.y * m_moveSpeed * dt;

	// zoom → z
	pos.z = m_baseZoom / m_zoom;

	transform.setPosition(pos);
	m_moveInput = {0, 0}; // 每帧清零（由外部 KeyDown 累加）
}
