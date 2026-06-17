#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>

#include "CameraTransform.h"
#include "IProjectionStrategy.h"
#include "IControlStrategy.h"
#include "PerspectiveProjection.h"  // setPerpective 兼容层需要

// 向后兼容：保留旧的全局变量声明（实际已弃用，仅用于过渡期编译）
// 新代码不应使用这些变量
extern glm::vec3 m_position;
extern float m_angle;

// 向后兼容：保留旧的自由函数声明（实际已弃用）
glm::vec3 get_ray_direction(int mouse_x, int mouse_y, int screen_width, int screen_height, glm::mat4 view_matrix, glm::mat4 projection_matrix);

enum class CAMERA_MOVE
{
	MOVE_LEFT,
	MOVE_RIGHT,
	MOVE_FRONT,
	MOVE_BACK
};

class Camera {
public:
	// === 构造 ===
	Camera();
	explicit Camera(std::unique_ptr<IProjectionStrategy> projStrategy,
					std::unique_ptr<IControlStrategy> controlStrategy);
	~Camera() = default;

	// 不允许拷贝（持有 unique_ptr）
	Camera(const Camera&) = delete;
	Camera& operator=(const Camera&) = delete;

	// === 策略替换（运行时热切换）===
	template<typename S, typename... Args>
	S* setProjectionStrategy(Args&&... args) {
		auto ptr = std::make_unique<S>(std::forward<Args>(args)...);
		S* raw = ptr.get();
		m_projection = std::move(ptr);
		return raw;
	}
	template<typename S, typename... Args>
	S* setControlStrategy(Args&&... args) {
		auto ptr = std::make_unique<S>(std::forward<Args>(args)...);
		S* raw = ptr.get();
		m_control = std::move(ptr);
		return raw;
	}

	// === 组件访问 ===
	CameraTransform& transform() { return m_transform; }
	const CameraTransform& transform() const { return m_transform; }
	IProjectionStrategy& projection() { return *m_projection; }
	const IProjectionStrategy& projection() const { return *m_projection; }
	IControlStrategy& controller() { return *m_control; }
	const IControlStrategy& controller() const { return *m_control; }

	// === 核心更新 ===
	void update(float dt);
	// 向后兼容：无参数版本（使用默认 dt=0）
	void update() { update(0.0f); }

	// === 矩阵 ===
	glm::mat4 getViewMatrix() const;
	glm::mat4 getProjectionMatrix() const;
	glm::mat4 getProjectionMatrix(float customAspectRatio) const;

	// === 坐标转换 ===
	glm::vec3 screenToWorld(int mouseX, int mouseY, int screenWidth, int screenHeight) const;

	// === 缩放快捷 ===
	void setZoom(float z);
	float getZoom() const;

	// === 视口设置 ===
	void setViewportSize(float width, float height);
	float getAspectRatio() const;

	// === 向后兼容接口（旧 API，内部委托给新组件）===
	glm::vec3 getCameraPos() { return m_transform.getPosition(); }
	void setCameraPos(glm::vec2 pos) { m_transform.setPosition(pos); m_position = m_transform.getPosition(); }
	void setCameraPos(glm::vec3 pos) { m_transform.setPosition(pos); m_position = pos; }

	void lookAt(glm::vec3 _pos, glm::vec3 _front, glm::vec3 _up) {
		m_transform.setPosition(_pos);
		m_transform.lookAt(_pos + _front, _up);
		m_position = _pos;
	}

	void setPerpective(float angle, float ratio, float nearPlane, float farPlane) {
		// 向后兼容：设置透视投影参数
		if (auto* p = dynamic_cast<PerspectiveProjection*>(m_projection.get())) {
			p->setFov(angle);
			p->setNearFar(nearPlane, farPlane);
		}
		m_viewportWidth = 1920.0f;
		m_viewportHeight = 1920.0f / ratio;
	}

	glm::mat4 getProjectMatrix() { return getProjectionMatrix(); }

	void move(CAMERA_MOVE _mode);
	void moveXY();

	void pitch(float _yOffset);
	void yaw(float _xOffset);
	void setSentitivity(float _s) { m_sensitivity = _s; }
	void onMouseMove(double _xpos, double _ypos);

private:
	CameraTransform m_transform;
	std::unique_ptr<IProjectionStrategy> m_projection;
	std::unique_ptr<IControlStrategy> m_control;

	// 视口
	float m_viewportWidth = 1920.0f;
	float m_viewportHeight = 1080.0f;

	// 向后兼容：旧 Camera 的鼠标控制状态
	float m_pitch = 0.0f;
	float m_yaw = -90.0f;
	float m_sensitivity = 0.1f;
	float m_xpos = 0;
	float m_ypos = 0;
	bool m_firstMove = true;
	glm::vec3 m_front{0, 0, -1};
	glm::vec3 m_up{0, 1, 0};
	float m_speed = 0;
	float m_speedX = 0;
	float m_speedY = 0;
};
