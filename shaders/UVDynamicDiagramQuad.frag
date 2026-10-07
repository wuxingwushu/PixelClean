#version 450

#extension GL_ARB_separate_shader_objects:enable

// 与 UVDynamicDiagram.frag 完全相同（单独一份是为了让回退管线自成一个 shader 集合）。
// 顶点着色器 UVDynamicDiagramQuad.vert 输出 location=3 的颜色，这里读取。

layout(location = 3) in vec4 inColor;
layout(location = 0) out vec4 outColor;

void main() {
	outColor = inColor;
}
