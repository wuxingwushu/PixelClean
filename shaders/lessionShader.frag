#version 450

#extension GL_ARB_separate_shader_objects:enable

layout(location = 1) in vec2 inUV;
layout(location = 3) in vec3 inColour;
layout(location = 4) in float inStrike;

layout(location = 0) out vec4 outColor;

layout(binding = 2) uniform sampler2D texSampler;

void main() {
	vec4 Color = texture(texSampler, inUV);
	if(inStrike != 0){
		float StrikeColour2 = 1.0 - inStrike;
		Color.x *= StrikeColour2;
		Color.y *= StrikeColour2;
		Color.z *= StrikeColour2;
		Color.x += inColour.x * inStrike;
		Color.y += inColour.y * inStrike;
		Color.z += inColour.z * inStrike;
	}
	outColor = Color;
}