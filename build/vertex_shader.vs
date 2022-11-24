#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 fragWorldPos;
out vec3 normal;
out vec2 texCoords;

uniform mat4 modelMatrix;
uniform mat3 normalMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

void main()
{
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.f);
    fragWorldPos = vec3(modelMatrix * vec4(aPos, 1.f));
    normal = normalize(normalMatrix * aNormal);
    texCoords = aTexCoords;
}