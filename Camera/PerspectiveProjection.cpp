#include "PerspectiveProjection.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

PerspectiveProjection::PerspectiveProjection(float fovDeg, float nearPlane, float farPlane)
	: m_fov(fovDeg), m_near(nearPlane), m_far(farPlane) {}

void PerspectiveProjection::setFov(float fovDeg) { m_fov = fovDeg; }
float PerspectiveProjection::getFov() const { return m_fov; }
void PerspectiveProjection::setNearFar(float nearPlane, float farPlane) { m_near = nearPlane; m_far = farPlane; }

glm::mat4 PerspectiveProjection::computeProjection(float aspectRatio) const {
	// zoom 体现为 FOV 缩放：zoom=2 → FOV 减半（更窄）, zoom=0.5 → FOV 加倍（更宽）
	float effectiveFov = m_fov / m_zoom;
	return glm::perspective(glm::radians(effectiveFov), aspectRatio, m_near, m_far);
}

void PerspectiveProjection::setZoom(float zoom) {
	m_zoom = std::clamp(zoom, m_zoomMin, m_zoomMax);
}

float PerspectiveProjection::getZoom() const { return m_zoom; }

void PerspectiveProjection::setZoomRange(float min, float max) {
	m_zoomMin = min;
	m_zoomMax = max;
}
