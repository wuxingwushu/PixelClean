#pragma once
//矩阵计算库
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

//获取屏幕位置对于的单位向量
glm::vec3 get_ray_direction(int mouse_x, int mouse_y, int screen_width, int screen_height, glm::mat4 view_matrix, glm::mat4 projection_matrix);

enum class CAMERA_MOVE
{
	MOVE_LEFT,
	MOVE_RIGHT,
	MOVE_FRONT,
	MOVE_BACK
};

extern glm::vec3	m_position;//位置
extern float		m_angle;//角度

class Camera
{
private:
	glm::vec3	m_front;
	glm::vec3	m_up;
	float		m_speed;
	float		m_speedX;
	float		m_speedY;

	float		m_pitch;
	float		m_yaw;
	float		m_sensitivity;

	float		m_xpos;
	float       m_ypos;
	bool		m_firstMove;

	glm::mat4	m_vMatrix;
	glm::mat4	m_pMatrx;

public:
	Camera()
	{
		m_position = glm::vec3(1.0f);
		m_front = glm::vec3(1.0f);
		m_up = glm::vec3(1.0f);
		m_speedX = 0.01f;
		m_speedY = 0.01f;

		m_pitch = 0.0f;
		m_yaw = -90.0f;
		m_sensitivity = 0.1f;

		m_xpos = 0;
		m_ypos = 0;
		m_firstMove = true;

		m_vMatrix = glm::mat4(1.0f);
	}
	~Camera()
	{

	}
	void lookAt(glm::vec3 _pos, glm::vec3 _front, glm::vec3 _up);
	void update();

	glm::vec3 getCameraPos();

	void setCameraPos(glm::vec2 pos);

	glm::mat4 getViewMatrix();

	glm::mat4 getProjectMatrix();

	void setSpeed(float _speed)
	{
		m_speed = _speed;
	}

	void setSpeedX(float _speed)
	{
		m_speedX = _speed;
	}

	void setSpeedY(float _speed)
	{
		m_speedY = _speed;
	}

	void move(CAMERA_MOVE _mode);
	void moveXY();

	void pitch(float _yOffset);
	void yaw(float _xOffset);
	void setSentitivity(float _s);
	void onMouseMove(double _xpos, double _ypos);

	void setPerpective(float angle, float ratio, float near, float far);
};
