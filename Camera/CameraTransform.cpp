#include "CameraTransform.h"

void CameraTransform::setPosition(const glm::vec3& pos) {
	m_position = pos;
	m_dirty = true;
}

void CameraTransform::setPosition(const glm::vec2& pos) {
	m_position.x = pos.x;
	m_position.y = pos.y;
	m_dirty = true;
}

glm::vec3 CameraTransform::getPosition() const {
	return m_position;
}

void CameraTransform::setAngle2D(float angleRad) {
	m_angle2D = angleRad;
	// 2D 顶视角不需要重建 view matrix（由 setPosition 触发）
}

float CameraTransform::getAngle2D() const {
	return m_angle2D;
}

void CameraTransform::setEuler(float pitch, float yaw) {
	m_pitch = pitch;
	m_yaw = yaw;
	// 重新计算 front 向量
	// 约定与 BlockWorld 一致：front.x = cos(pitch)*sin(yaw), front.z = cos(pitch)*cos(yaw)
	// 注意：BlockWorld 用 sin(yaw) for x, cos(yaw) for z；此处保持一致
	m_front.x = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	m_front.y = sin(glm::radians(m_pitch));
	m_front.z = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	m_front = glm::normalize(m_front);
	m_dirty = true;
}

float CameraTransform::getPitch() const { return m_pitch; }
float CameraTransform::getYaw() const { return m_yaw; }
glm::vec3 CameraTransform::getForward() const { return m_front; }

void CameraTransform::lookAt(const glm::vec3& target, const glm::vec3& up) {
	// 保持位置不变，调整 front 指向 target
	auto dir = target - m_position;
	m_front = glm::normalize(dir);
	m_up = up;
	m_dirty = true;
}

glm::mat4 CameraTransform::getViewMatrix() const {
	if (m_dirty) rebuildViewMatrix();
	return m_viewMatrix;
}

void CameraTransform::rebuildViewMatrix() const {
	m_viewMatrix = glm::lookAt(m_position, m_position + m_front, m_up);
	m_dirty = false;
}
