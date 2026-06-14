#version 450

#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

void main() {
    // 光照方向（视线空间中从上方偏斜照射，模拟太阳光）
    vec3 lightDir = normalize(vec3(0.3, 1.0, 0.5));

    // 环境光
    float ambient = 0.35;

    // 漫反射
    float diffuse = max(dot(normalize(inNormal), lightDir), 0.0);

    // 最终光照强度
    float lighting = ambient + diffuse * 0.65;

    outColor = vec4(inColor.rgb * lighting, inColor.a);
}
