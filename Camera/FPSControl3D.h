#pragma once
#include "IControlStrategy.h"

class FPSControl3D : public IControlStrategy {
public:
	FPSControl3D(float sensitivity = 0.1f, float moveSpeed = 20.0f);

	void setMoveSpeed(float speed);
	float getMoveSpeed() const;

	// IControlStrategy
	void update(float dt, CameraTransform& transform) override;
	void onMouseMove(double xpos, double ypos) override;
	void onScroll(float delta) override;
	void onKeyMove(glm::vec2 direction, float speed) override;
	const char* name() const override { return "FPS3D"; }

private:
	float m_sensitivity = 0.1f;
	float m_moveSpeed = 20.0f;
	glm::vec2 m_moveInput{0};
	double m_lastX = 0, m_lastY = 0;
	bool m_firstMove = true;
	float m_yawAccum = 0.0f;
	float m_pitchAccum = 0.0f;
};
