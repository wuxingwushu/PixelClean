#version 450

#extension GL_ARB_separate_shader_objects:enable

// 粒子系统专用片段着色器（方案 D）
// 旧 shader 用 1×1 纹理采样得到纯色，现在直接用顶点传入颜色
// UV 在 [0,1] 四边形内，无需纹理

layout(location = 0) in  vec2 fragUV;
layout(location = 1) in  vec4 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = fragColor;
}
