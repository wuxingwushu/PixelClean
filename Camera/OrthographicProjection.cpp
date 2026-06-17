#include "OrthographicProjection.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

OrthographicProjection::OrthographicProjection(float size, float nearPlane, float farPlane)
	: m_size(size), m_near(nearPlane), m_far(farPlane) {}

void OrthographicProjection::setSize(float size) { m_size = size; }
float OrthographicProjection::getSize() const { return m_size; }
void OrthographicProjection::setNearFar(float nearPlane, float farPlane) { m_near = nearPlane; m_far = farPlane; }

glm::mat4 OrthographicProjection::computeProjection(float aspectRatio) const {
	// zoom：m_size / zoom 控制可见范围；m_zoom 越大可见范围越小（放大）
	float effectiveSize = m_size / m_zoom;
	float left = -effectiveSize * aspectRatio;
	float right = effectiveSize * aspectRatio;
	float bottom = -effectiveSize;
	float top = effectiveSize;
	return glm::ortho(left, right, bottom, top, m_near, m_far);
}

void OrthographicProjection::setZoom(float zoom) {
	m_zoom = std::clamp(zoom, m_zoomMin, m_zoomMax);
}

float OrthographicProjection::getZoom() const { return m_zoom; }

void OrthographicProjection::setZoomRange(float min, float max) {
	m_zoomMin = min;
	m_zoomMax = max;
}
