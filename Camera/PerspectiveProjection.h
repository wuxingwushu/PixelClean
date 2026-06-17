#pragma once
#include "IProjectionStrategy.h"

class PerspectiveProjection : public IProjectionStrategy {
public:
	PerspectiveProjection(float fovDeg = 45.0f, float nearPlane = 0.1f, float farPlane = 1000.0f);

	void setFov(float fovDeg);
	float getFov() const;

	void setNearFar(float nearPlane, float farPlane);

	// IProjectionStrategy
	glm::mat4 computeProjection(float aspectRatio) const override;
	void setZoom(float zoom) override;
	float getZoom() const override;
	void setZoomRange(float min, float max) override;
	const char* name() const override { return "Perspective"; }

private:
	float m_fov = 45.0f;       // 度
	float m_near = 0.1f;
	float m_far = 1000.0f;
	float m_zoom = 1.0f;
	float m_zoomMin = 0.1f;
	float m_zoomMax = 100.0f;
};
