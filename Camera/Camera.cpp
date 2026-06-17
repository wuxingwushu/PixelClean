#include "Camera.h"
#include "PerspectiveProjection.h"
#include "OrthographicProjection.h"
#include "FollowControl2D.h"
#include <algorithm>

// === 向后兼容：全局变量定义（保留旧代码兼容）===
glm::vec3 m_position{0, 0, 400};
float m_angle = 0.0f;

// === 向后兼容：自由函数 ===
glm::vec3 get_ray_direction(int mouse_x, int mouse_y, int screen_width, int screen_height, glm::mat4 view_matrix, glm::mat4 projection_matrix) {
	float x = (2.0f * mouse_x) / screen_width - 1.0f;
	float y = 1.0f - (2.0f * mouse_y) / screen_height;

	glm::vec4 clip_coords(x, y, -1.0f, 1.0f);
	glm::vec4 eye_coords = glm::inverse(projection_matrix) * clip_coords;
	eye_coords = glm::vec4(eye_coords.x, eye_coords.y, -1.0f, 0.0f);
	glm::vec4 world_coords = glm::inverse(view_matrix) * eye_coords;
	return glm::normalize(glm::vec3(world_coords));
}

// === 默认构造：兼容旧代码构造方式 ===
Camera::Camera()
	: m_projection(std::make_unique<PerspectiveProjection>(45.0f, 0.1f, 1000.0f))
	, m_control(std::make_unique<FollowControl2D>(5.0f)) {
	m_transform.setPosition({20.0f, 20.0f, 400.0f});
	// lookAt 语义：_pos + _front = target，传入 front={0,0,-1} 表示从上方俯视
	m_transform.lookAt(glm::vec3(20.0f, 20.0f, 399.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

Camera::Camera(std::unique_ptr<IProjectionStrategy> projStrategy,
			   std::unique_ptr<IControlStrategy> controlStrategy)
	: m_projection(std::move(projStrategy))
	, m_control(std::move(controlStrategy)) {}

// === 核心更新 ===
void Camera::update(float dt) {
	// 第一步：同步全局变量 → CameraTransform（兼容旧代码直接改 m_position/m_angle）
	m_transform.setPosition(m_position);
	m_transform.setAngle2D(m_angle);

	// 第二步：运行控制策略（可能修改 transform）
	if (m_control) {
		m_control->update(dt, m_transform);
	}
	// 第三步：同步 CameraTransform → 全局变量（新 API 修改反映回全局变量）
	m_position = m_transform.getPosition();
	m_angle = m_transform.getAngle2D();
}

// === 矩阵 ===
glm::mat4 Camera::getViewMatrix() const {
	return m_transform.getViewMatrix();
}

glm::mat4 Camera::getProjectionMatrix() const {
	return getProjectionMatrix(m_viewportWidth / m_viewportHeight);
}

glm::mat4 Camera::getProjectionMatrix(float customAspectRatio) const {
	if (m_projection) {
		return m_projection->computeProjection(customAspectRatio);
	}
	return glm::mat4(1.0f);
}

// === 坐标转换 ===
glm::vec3 Camera::screenToWorld(int mouseX, int mouseY, int screenWidth, int screenHeight) const {
	float x = (2.0f * mouseX) / screenWidth - 1.0f;
	float y = 1.0f - (2.0f * mouseY) / screenHeight;

	glm::mat4 view = getViewMatrix();
	glm::mat4 proj = getProjectionMatrix();

	glm::vec4 clipCoords(x, y, -1.0f, 1.0f);
	glm::vec4 eyeCoords = glm::inverse(proj) * clipCoords;
	eyeCoords = glm::vec4(eyeCoords.x, eyeCoords.y, -1.0f, 0.0f);
	glm::vec4 worldCoords = glm::inverse(view) * eyeCoords;
	return glm::normalize(glm::vec3(worldCoords));
}

// === 缩放快捷 ===
void Camera::setZoom(float z) {
	if (m_projection) m_projection->setZoom(z);
}
float Camera::getZoom() const {
	return m_projection ? m_projection->getZoom() : 1.0f;
}

// === 视口 ===
void Camera::setViewportSize(float width, float height) {
	m_viewportWidth = width;
	m_viewportHeight = height;
}
float Camera::getAspectRatio() const {
	return m_viewportWidth / m_viewportHeight;
}

// === 向后兼容：旧 API 实现 ===
void Camera::move(CAMERA_MOVE _mode) {
	glm::vec3 pos = m_transform.getPosition();
	switch (_mode) {
	case CAMERA_MOVE::MOVE_LEFT:
		pos -= m_speed * glm::vec3(1.0f, 0.0f, 0.0f);
		break;
	case CAMERA_MOVE::MOVE_RIGHT:
		pos += m_speed * glm::vec3(1.0f, 0.0f, 0.0f);
		break;
	case CAMERA_MOVE::MOVE_FRONT:
		pos += m_speed * glm::vec3(0.0f, 1.0f, 0.0f);
		break;
	case CAMERA_MOVE::MOVE_BACK:
		pos -= m_speed * glm::vec3(0.0f, 1.0f, 0.0f);
		break;
	default:
		break;
	}
	m_transform.setPosition(pos);
}

void Camera::moveXY() {
	glm::vec3 pos = m_transform.getPosition();
	if (m_speedX != 0.0f) {
		pos += m_speedX * glm::vec3(1.0f, 0.0f, 0.0f);
	}
	if (m_speedY != 0.0f) {
		pos += m_speedY * glm::vec3(0.0f, 1.0f, 0.0f);
	}
	m_transform.setPosition(pos);
}

void Camera::pitch(float _yOffset) {
	m_pitch += _yOffset * m_sensitivity;
	if (m_pitch >= 89.0f) m_pitch = 89.0f;
	if (m_pitch <= -89.0f) m_pitch = -89.0f;

	m_front.y = sin(glm::radians(m_pitch));
	m_front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	m_front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	m_front = glm::normalize(m_front);
	m_transform.lookAt(m_transform.getPosition() + m_front, m_up);
}

void Camera::yaw(float _xOffset) {
	m_yaw += _xOffset * m_sensitivity;
	m_front.y = sin(glm::radians(m_pitch));
	m_front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	m_front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	m_front = glm::normalize(m_front);
	m_transform.lookAt(m_transform.getPosition() + m_front, m_up);
}

void Camera::onMouseMove(double _xpos, double _ypos) {
	if (m_firstMove) {
		m_xpos = (float)_xpos;
		m_ypos = (float)_ypos;
		m_firstMove = false;
		return;
	}
	float _xOffset = (float)_xpos - m_xpos;
	float _yOffset = -((float)_ypos - m_ypos);
	m_xpos = (float)_xpos;
	m_ypos = (float)_ypos;
	pitch(_yOffset);
	yaw(_xOffset);
}
