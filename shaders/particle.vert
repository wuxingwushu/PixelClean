#version 450

#extension GL_ARB_separate_shader_objects:enable

// 粒子系统专用顶点着色器（方案 D：SSBO + Instanced Rendering）
// 实例数据从 SSBO 读取，靠 gl_InstanceIndex 索引，不使用 instance binding

layout(location = 0) in vec3  inPosition;
layout(location = 2) in vec2  inUV;

layout(location = 0) out vec2  fragUV;
layout(location = 1) out vec4  fragColor;

layout(binding = 0) uniform VPMatrices {
    mat4 mViewMatrix;
    mat4 mProjectionMatrix;
} vpUBO;

// SSBO 中单个粒子的数据，std140 对齐
// 总大小: 64 (mat4) + 16 (vec4) = 80 字节
struct ParticleInstanceData {
    mat4 modelMatrix;
    vec4 color;
};

layout(std140, binding = 1) readonly buffer InstanceBuffer {
    ParticleInstanceData instances[];
};

void main() {
    ParticleInstanceData inst = instances[gl_InstanceIndex];
    gl_Position = vpUBO.mProjectionMatrix * vpUBO.mViewMatrix
                * inst.modelMatrix * vec4(inPosition, 1.0);
    fragUV    = inUV;
    fragColor = inst.color;
}
