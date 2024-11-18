#version 450

#extension GL_ARB_separate_shader_objects:enable

layout(location = 0) in vec2 inPosition;
layout(location = 1) in int inIndex;

layout(location = 2) out vec4 outColor;
layout(location = 4) out mat4 outViewMatrix;

layout(binding = 0) uniform VPMatrices {
	mat4 mViewMatrix;
	mat4 mProjectionMatrix;
}vpUBO;

layout(binding = 1) uniform ObjectUniform {
	mat4 mModelMatrix;
	uint CellSize;
	uint Frame;
}objectUBO;

layout(std430, binding = 2) buffer BasisChart//基础原图
{
   vec4 Color[];
};

layout(std430, binding = 3) buffer UVIndex//UV索引
{
   int Index[];
};

void main() {
	gl_Position = vec4(inPosition, 0.0, 1.0);
	int shu = Index[((objectUBO.CellSize * objectUBO.Frame) + inIndex)];
	if(shu < 0){
		outColor = vec4(0.0, 0.0, 0.0, 0.0);
	}else{
		outColor = Color[shu];
	}
	outViewMatrix = vpUBO.mProjectionMatrix * vpUBO.mViewMatrix * objectUBO.mModelMatrix;	
}