#pragma once
#include "IControlStrategy.h"

class FixedControl : public IControlStrategy {
public:
	explicit FixedControl(const glm::vec3& fixedPos = {0, 0, 400});

	void setFixedPosition(const glm::vec3& pos);

	// IControlStrategy
	void update(float dt, CameraTransform& transform) override;
	const char* name() const override { return "Fixed"; }

private:
	glm::vec3 m_fixedPosition;
};
