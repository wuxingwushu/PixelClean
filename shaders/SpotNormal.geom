#version 450 core
layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 3) in vec4 inGeomColor[];
layout(location = 4) in mat4 inViewMatrix[];
layout(location = 8) in float inSize[]; // 新增：接收大小参数

layout(location = 9) out vec4 outColor;

void main() {
    vec4 position = gl_in[0].gl_Position;
    if (position.z > -1.0) {
        float size = inSize[0]; // 使用动态大小
        vec4 Horn = vec4(size, size, 0.0, 0.0) * inViewMatrix[0]; // 替换固定值

        outColor = inGeomColor[0];
        gl_Position = vec4(position.x - Horn.x, position.y - Horn.y, position.z, position.w);
        EmitVertex();

        outColor = inGeomColor[0];
        gl_Position = vec4(position.x - Horn.x, position.y + Horn.y, position.z, position.w);
        EmitVertex();

        outColor = inGeomColor[0];
        gl_Position = vec4(position.x + Horn.x, position.y - Horn.y, position.z, position.w);
        EmitVertex();

        outColor = inGeomColor[0];
        gl_Position = vec4(position.x + Horn.x, position.y + Horn.y, position.z, position.w);
        EmitVertex();
    }
    EndPrimitive();
}
