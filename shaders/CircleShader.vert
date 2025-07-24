#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in float inRadius;
layout(location = 2) in vec4 inColor;

layout(location = 3) out vec4 outColor;
layout(location = 4) out mat4 outViewMatrix;
layout(location = 8) out float outRadius;

layout(binding = 0) uniform VPMatrices {
    mat4 mViewMatrix;
    mat4 mProjectionMatrix;
} vpUBO;

void main() {
    outColor = inColor;
    outViewMatrix = vpUBO.mProjectionMatrix;
    outRadius = inRadius; // 传递 size
    gl_Position = vpUBO.mProjectionMatrix * vpUBO.mViewMatrix * vec4(inPosition, 1.0f);
}