#version 450

#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform VPMatrices {
    mat4 mViewMatrix;
    mat4 mProjectionMatrix;
} vpUBO;

void main() {
    gl_Position = vpUBO.mProjectionMatrix * vpUBO.mViewMatrix * vec4(inPosition, 1.0f);
    outColor = inColor;
}