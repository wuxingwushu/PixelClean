#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 9) in vec4 inColor; // 注意 location 更新为 6
layout(location = 0) out vec4 outColor;

void main() {
    outColor = inColor;
}
