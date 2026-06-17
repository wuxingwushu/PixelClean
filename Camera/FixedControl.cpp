#include "FixedControl.h"
#include "CameraTransform.h"

FixedControl::FixedControl(const glm::vec3& fixedPos) : m_fixedPosition(fixedPos) {}

void FixedControl::setFixedPosition(const glm::vec3& pos) { m_fixedPosition = pos; }

void FixedControl::update(float dt, CameraTransform& transform) {
	transform.setPosition(m_fixedPosition);
}
