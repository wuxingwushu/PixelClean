#include "FPSControl3D.h"
#include "CameraTransform.h"
#include <algorithm>
#include <cmath>

FPSControl3D::FPSControl3D(float sensitivity, float moveSpeed)
	: m_sensitivity(sensitivity), m_moveSpeed(moveSpeed) {}

void FPSControl3D::setMoveSpeed(float speed) { m_moveSpeed = speed; }
float FPSControl3D::getMoveSpeed() const { return m_moveSpeed; }

void FPSControl3D::onKeyMove(glm::vec2 direction, float speed) {
	m_moveInput = direction;
	if (speed > 0) m_moveSpeed = speed;
}

void FPSControl3D::onMouseMove(double xpos, double ypos) {
	if (m_firstMove) {
		m_lastX = xpos;
		m_lastY = ypos;
		m_firstMove = false;
		return;
	}
	float xOffset = static_cast<float>(xpos - m_lastX);
	float yOffset = static_cast<float>(-(ypos - m_lastY));
	m_lastX = xpos;
	m_lastY = ypos;

	// 暂存偏移，在 update 中应用
	m_yawAccum += xOffset * m_sensitivity;
	m_pitchAccum += yOffset * m_sensitivity;
}

void FPSControl3D::onScroll(float delta) {
	// zoom in/out via FOV（BlockWorld 不使用此功能，滚轮改速度）
	m_moveSpeed = std::clamp(m_moveSpeed + delta * 5.0f, 1.0f, 200.0f);
}

void FPSControl3D::update(float dt, CameraTransform& transform) {
	// 应用鼠标视角
	float pitch = transform.getPitch() + m_pitchAccum;
	float yaw = transform.getYaw() + m_yawAccum;
	pitch = std::clamp(pitch, -89.0f, 89.0f);
	transform.setEuler(pitch, yaw);
	m_yawAccum = 0.0f;
	m_pitchAccum = 0.0f;

	// 应用移动
	glm::vec3 pos = transform.getPosition();
	glm::vec3 forward = transform.getForward();
	pos += forward * m_moveInput.y * m_moveSpeed * dt;
	// 横向移动（使用 forward 叉乘 up）
	glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
	pos += right * m_moveInput.x * m_moveSpeed * dt;
	transform.setPosition(pos);

	m_moveInput = {0, 0};
}
