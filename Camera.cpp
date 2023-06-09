#include "Camera.h"

glm::vec3	m_position;
float		m_angle;

void Camera::lookAt(glm::vec3 _pos, glm::vec3 _front, glm::vec3 _up)
{
	m_position = _pos;
	m_front = glm::normalize(_front);
	m_up = _up;

	m_vMatrix = glm::lookAt(m_position, m_position + m_front, m_up);
}

void Camera::update()
{
	m_vMatrix = glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::vec3 Camera::getCameraPos() {
	return m_position;
}

void Camera::setCameraPos(glm::vec2 pos) {
	m_position.x = pos.x;
	m_position.y = pos.y;

	update();
}

glm::mat4 Camera::getViewMatrix()
{
	return m_vMatrix;
}

glm::mat4 Camera::getProjectMatrix() {
	return m_pMatrx;
}

void Camera::move(CAMERA_MOVE _mode)
{
	switch (_mode)
	{
	//��
	case CAMERA_MOVE::MOVE_LEFT:
		m_position -= m_speed * glm::vec3(1.0f, 0.0f, 0.0f);
		break;
	//��
	case CAMERA_MOVE::MOVE_RIGHT:
		m_position += m_speed * glm::vec3(1.0f, 0.0f, 0.0f);
		break;
	//��
	case CAMERA_MOVE::MOVE_FRONT:
		m_position += m_speed * glm::vec3(0.0f, 1.0f, 0.0f);
		break;
	//��
	case CAMERA_MOVE::MOVE_BACK:
		m_position -= m_speed * glm::vec3(0.0f, 1.0f, 0.0f);
		break;
	default:
		break;
	}

	update();
}

void Camera::moveXY()
{
	if (m_speedX != 0.0f) {
		m_position += m_speedX * glm::vec3(1.0f, 0.0f, 0.0f);
	}
	if (m_speedY != 0.0f) {
		m_position += m_speedY * glm::vec3(0.0f, 1.0f, 0.0f);
	}
	if ((m_speedX != 0.0f) || (m_speedY != 0.0f)) {
		update();
	}
}

void Camera::pitch(float _yOffset)
{
	m_pitch += _yOffset * m_sensitivity;

	if (m_pitch >= 89.0f)
	{
		m_pitch = 89.0f;
	}

	if (m_pitch <= -89.0f)
	{
		m_pitch = -89.0f;
	}

	m_front.y = sin(glm::radians(m_pitch));
	m_front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	m_front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));

	m_front = glm::normalize(m_front);
	update();
}
void Camera::yaw(float _xOffset)
{
	m_yaw += _xOffset * m_sensitivity;

	m_front.y = sin(glm::radians(m_pitch));
	m_front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	m_front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));

	m_front = glm::normalize(m_front);
	update();
}
void Camera::setSentitivity(float _s)
{
	m_sensitivity = _s;
}

void Camera::setPerpective(float angle, float ratio, float near, float far) {
	m_pMatrx = glm::perspective(glm::radians(angle), ratio, near, far);
}

void Camera::onMouseMove(double _xpos, double _ypos)
{
	if (m_firstMove)
	{
		m_xpos = _xpos;
		m_ypos = _ypos;
		m_firstMove = false;
		return;
	}

	float _xOffset = _xpos - m_xpos;
	float _yOffset = -(_ypos - m_ypos);

	m_xpos = _xpos;
	m_ypos = _ypos;

	pitch(_yOffset);
	yaw(_xOffset);
}
