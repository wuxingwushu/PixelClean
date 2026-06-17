#pragma once
#include "IControlStrategy.h"

class FreeControl2D : public IControlStrategy {
public:
	FreeControl2D(float moveSpeed = 20.0f);

	void setMoveSpeed(float speed);

	// IControlStrategy
	void update(float dt, CameraTransform& transform) override;
	void onScroll(float delta) override;
	void onKeyMove(glm::vec2 direction, float speed) override;
	const char* name() const override { return "Free2D"; }

private:
	float m_moveSpeed = 20.0f;
	glm::vec2 m_moveInput{0};

	// 缩放
	float m_zoom = 1.0f;
	float m_zoomMin = 0.1f;
	float m_zoomMax = 100.0f;
	float m_baseZoom = 400.0f;
};
