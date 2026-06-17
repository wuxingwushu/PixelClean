#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class CameraTransform {
public:
	CameraTransform() = default;

	// === 位置 ===
	void setPosition(const glm::vec3& pos);
	void setPosition(const glm::vec2& pos);  // z 不变
	glm::vec3 getPosition() const;

	// === 2D 角度（顶视角）===
	void setAngle2D(float angleRad);
	float getAngle2D() const;

	// === 3D 欧拉角（FPS）===
	void setEuler(float pitch, float yaw);
	float getPitch() const;
	float getYaw() const;
	glm::vec3 getForward() const;

	// === 视点/注视 ===
	void lookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0, 0, 1));

	// === 矩阵（需在每帧 update 后调用）===
	glm::mat4 getViewMatrix() const;

	// === 脏标记（外部调用后标记）===
	void markDirty() { m_dirty = true; }

private:
	void rebuildViewMatrix() const;

	// 位置
	glm::vec3 m_position{0, 0, 400};

	// 2D 模式
	float m_angle2D = 0.0f;            // 玩家瞄准角度（顶视角用）

	// 3D 模式
	float m_pitch = 0.0f;
	float m_yaw = -90.0f;
	glm::vec3 m_front{0, 0, -1};
	glm::vec3 m_up{0, 1, 0};

	// 矩阵缓存
	mutable glm::mat4 m_viewMatrix{1.0f};
	mutable bool m_dirty = true;
};
