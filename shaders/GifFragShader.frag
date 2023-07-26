#version 450

#extension GL_ARB_separate_shader_objects:enable

layout(location = 1) in vec2 inUV;
layout(location = 2) in flat uint inzhen;
layout(location = 3) in float inchuang;

layout(location = 0) out vec4 outColor;

layout(binding = 2) uniform sampler2D texSampler;

vec2 UV;

void main() {
	UV.y = inUV.y;
	UV.x = (inchuang * inzhen) + inUV.x;
	if(UV.x > 1.0f){UV.x -= 1.0f;}
	outColor = texture(texSampler, UV);
}