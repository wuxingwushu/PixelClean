#version 450 core

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;//输出点最大数量

layout(location = 2) in vec4 inGeomColor[];
layout(location = 4) in mat4 inViewMatrix[];

layout(location = 3) out vec4 outColor;

void main()
{
    if(inGeomColor[0].w > 0){
        // 从输入顶点中获取位置信息
        vec4 position = gl_in[0].gl_Position;

        vec4 Horn = vec4(-1.0, -1.0, 0.0, 0.0);
        // 生成三角形的三个顶点
        outColor = inGeomColor[0];
        gl_Position = inViewMatrix[0] * vec4(position.x, position.y, position.z, position.w);
        EmitVertex();                 
        
        outColor = inGeomColor[0];
        gl_Position = inViewMatrix[0] * vec4(position.x, position.y + Horn.y, position.z, position.w);
        EmitVertex();
        
        outColor = inGeomColor[0];
        gl_Position = inViewMatrix[0] * vec4(position.x + Horn.x, position.y, position.z, position.w);
        EmitVertex();

        outColor = inGeomColor[0];
        gl_Position = inViewMatrix[0] * vec4(position.x + Horn.x, position.y + Horn.y, position.z, position.w);
        EmitVertex();
    }
    
    // 结束当前图元的绘制
    EndPrimitive();
}