#version 450

#extension GL_ARB_separate_shader_objects:enable

// 无几何着色器的回退版本：把 UVDynamicDiagram.geom 的"点 -> 方块"展开搬到顶点着色器里。
// 做法与几何着色器完全等价：
//   1. 顶点缓冲按实例步进读取，每个实例 = 原来的一个点（四边形中心）；
//   2. 每个实例画 4 个顶点（triangle_strip）拼成方块，顶点编号 0/1/2/3 对应原来的 4 次 EmitVertex；
//   3. 方块偏移固定为 vec2(-1,-1)（与 geom 里的 Horn 相同），保持画面尺寸/位置不变。
// 使用场景：SwiftShader / llvmpipe 等 CPU 软件设备不支持 geometryShader。

layout(location = 0) in vec2 inPosition;
layout(location = 1) in int inIndex;

// 与 UVDynamicDiagram.frag 匹配：它读取的是 location=3 的 vec4
//（原几何着色器是 vec4 in[] -> vec4 out，这里直接输出，语义相同）
layout(location = 3) out vec4 outColor;

layout(binding = 0) uniform VPMatrices {
	mat4 mViewMatrix;
	mat4 mProjectionMatrix;
}vpUBO;

layout(binding = 1) uniform ObjectUniform {
	mat4 mModelMatrix;
	uint CellSize;
	uint Frame;
}objectUBO;

layout(std430, binding = 2) readonly buffer BasisChart//基础原图 （readonly只读不写）
{
   vec4 Color[];
};

layout(std430, binding = 3) readonly buffer UVIndex//UV索引 （readonly只读不写）
{
   int Index[];
};

void main() {
	// 一个点展成方块，和 geom 里的 Horn 完全一致
	const vec2 Horn = vec2(-1.0, -1.0);

	// 顶点编号 0/1/2/3 -> 方块的四个角，顺序与 geom 的 EmitVertex 顺序一致
	int corner = gl_VertexIndex & 3;
	float dx = (corner == 2 || corner == 3) ? Horn.x : 0.0;
	float dy = (corner >= 1) ? Horn.y : 0.0;

	int shu = Index[((objectUBO.CellSize * objectUBO.Frame) + inIndex)];
	if (shu < 0) {
		outColor = vec4(0.0, 0.0, 0.0, 0.0);
	}
	else {
		outColor = Color[shu];
	}

	// 顶点位置 = 点位置 + 方块角偏移，然后走和几何着色器相同的 viewMatrix
	vec4 position = vec4(inPosition + vec2(dx, dy), 0.0, 1.0);
	mat4 viewMatrix = vpUBO.mProjectionMatrix * vpUBO.mViewMatrix * objectUBO.mModelMatrix;
	gl_Position = viewMatrix * position;
}
