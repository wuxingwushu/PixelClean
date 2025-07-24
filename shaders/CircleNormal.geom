#version 450 core
layout(points) in;
layout(line_strip, max_vertices = 64) out; // 线段模式

layout(location = 3) in vec4 inGeomColor[];
layout(location = 4) in mat4 inViewMatrix[];
layout(location = 8) in float inRadius[];

const int SEGMENTS = 60; // 60个线段连接成的圆形

layout(location = 9) out vec4 outColor;

void main() {
    vec4 position = gl_in[0].gl_Position;
    
    if (position.z > -1.0) {
        for (int i = 0; i < SEGMENTS + 1; ++i) {
            float angle = i * 2 * 3.14159265358979323846 / SEGMENTS;
            vec2 offset = vec2(cos(angle), sin(angle)) * inRadius[0];
            gl_Position = position + vec4(offset.x, offset.y, 0, 0) * inViewMatrix[0];
            outColor = inGeomColor[0];
            EmitVertex();
        }
    }
    EndPrimitive();
}