#pragma once
#include "IProjectionStrategy.h"

class OrthographicProjection : public IProjectionStrategy {
public:
	OrthographicProjection(float size = 10.0f, float nearPlane = 0.1f, float farPlane = 1000.0f);

	void setSize(float size);
	float getSize() const;

	void setNearFar(float nearPlane, float farPlane);

	// IProjectionStrategy
	glm::mat4 computeProjection(float aspectRatio) const override;
	void setZoom(float zoom) override;
	float getZoom() const override;
	void setZoomRange(float min, float max) override;
	const char* name() const override { return "Orthographic"; }

private:
	float m_size = 10.0f;       // 正交视口半高
	float m_near = 0.1f;
	float m_far = 1000.0f;
	float m_zoom = 1.0f;
	float m_zoomMin = 0.1f;
	float m_zoomMax = 100.0f;
};
