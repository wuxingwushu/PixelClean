#version 450
#extension GL_ARB_separate_shader_objects : enable

// 无几何着色器的回退版本：把 CircleNormal.geom 的展开搬到顶点着色器。
// 几何着色器做的事（每个点 -> 60 段 line_strip 组成的圆环）：
//   for i in [0, 60]:
//       angle = i * 2PI / 60
//       offset = vec2(cos(angle), sin(angle)) * radius
//       gl_Position = position + vec4(offset.x, offset.y, 0, 0) * mProjectionMatrix;
// 这里用实例化绘制实现：顶点缓冲按实例步进（每个实例 = 原来的一个点），
// 每个实例画 61 个顶点（line_strip，首尾闭合）画出同样的圆环。

layout(location = 0) in vec3 inPosition;
layout(location = 1) in float inRadius;
layout(location = 2) in vec4 inColor;

// 与 CircleShader.frag 匹配
layout(location = 9) out vec4 outColor;

layout(binding = 0) uniform VPMatrices {
    mat4 mViewMatrix;
    mat4 mProjectionMatrix;
} vpUBO;

const int SEGMENTS = 60; // 与 CircleNormal.geom 保持一致

void main() {
    // 顶点编号 i 对应几何着色器第 i 次 EmitVertex
    int segment = gl_VertexIndex;

    outColor = inColor;

    vec4 position = vpUBO.mProjectionMatrix * vpUBO.mViewMatrix * vec4(inPosition, 1.0f);
    if (position.z <= -1.0) {
        // 与 geom 的 if(position.z > -1.0) 等价：z <= -1 时那个点不产生任何顶点，这里退化成零面积图元
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float angle = float(segment) * 2.0 * 3.14159265358979323846 / float(SEGMENTS);
    vec2 offset = vec2(cos(angle), sin(angle)) * inRadius;
    gl_Position = position + vec4(offset.x, offset.y, 0.0, 0.0) * vpUBO.mProjectionMatrix;
}
