#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in float inSize; // 新增：控制大小的参数
layout(location = 2) in vec4 inColor; // 原 location=1 改为 location=2

layout(location = 3) out vec4 outColor;
layout(location = 4) out mat4 outViewMatrix;
layout(location = 8) out float outSize; // 传递 size 到几何着色器

layout(binding = 0) uniform VPMatrices {
    mat4 mViewMatrix;
    mat4 mProjectionMatrix;
} vpUBO;

void main() {
    outColor = inColor;
    outViewMatrix = vpUBO.mProjectionMatrix;
    outSize = inSize; // 传递 size
    gl_Position = vpUBO.mProjectionMatrix * vpUBO.mViewMatrix * vec4(inPosition, 1.0f);
}
