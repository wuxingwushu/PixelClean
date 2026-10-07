#version 450
#extension GL_ARB_separate_shader_objects : enable

// 无几何着色器的回退版本：把 SpotNormal.geom 的展开搬到顶点着色器。
// 几何着色器做的事（每个点 -> 一个 size x size 的方块）：
//   vec4 Horn = vec4(size, size, 0, 0) * mProjectionMatrix;
//   gl_Position = position + vec4(±Horn.x, ±Horn.y, 0, 0);
// 这里用实例化绘制实现：顶点缓冲按实例步进（每个实例 = 原来的一个点），
// 每个实例画 4 个顶点（triangle_strip）拼成方块，顶点编号 0/1/2/3 的
// 偏移顺序与 geom 里 4 次 EmitVertex 完全一致。变量名沿用 SpotShader.vert 的 vpUBO。

layout(location = 0) in vec3 inPosition;
layout(location = 1) in float inSize; // 控制大小的参数
layout(location = 2) in vec4 inColor;

// 与 SpotShader.frag 匹配
layout(location = 9) out vec4 outColor;

layout(binding = 0) uniform VPMatrices {
    mat4 mViewMatrix;
    mat4 mProjectionMatrix;
} vpUBO;

void main() {
    // 顶点编号 0/1/2/3 -> 方块的四个角，顺序与 geom 的 EmitVertex 顺序一致
    int corner = gl_VertexIndex & 3;
    float sx = (corner == 2 || corner == 3) ? 1.0 : -1.0;
    float sy = (corner >= 1) ? 1.0 : -1.0;

    outColor = inColor;

    vec4 position = vpUBO.mProjectionMatrix * vpUBO.mViewMatrix * vec4(inPosition, 1.0f);
    if (position.z <= -1.0) {
        // 与 geom 的 if(position.z > -1.0) 等价：z <= -1 时那个点不产生任何顶点，这里退化成零面积图元
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec4 Horn = vec4(inSize, inSize, 0.0, 0.0) * vpUBO.mProjectionMatrix;
    gl_Position = vec4(position.x + sx * Horn.x, position.y + sy * Horn.y, position.z, position.w);
}
