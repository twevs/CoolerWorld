#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in mat4 aModelMatrix;
layout (location = 7) in mat3 aNormalMatrix;

out vec3 fragWorldPos;
out vec3 normal;
out vec2 texCoords;

layout (std140, binding = 0) uniform Matrices
{
	mat4 viewMatrix;
	mat4 projectionMatrix;
};

void main()
{
    gl_Position = projectionMatrix * viewMatrix * aModelMatrix * vec4(aPos, 1.f);
    fragWorldPos = vec3(aModelMatrix * vec4(aPos, 1.f));
    normal = normalize(aNormalMatrix * aNormal);
    texCoords = aTexCoords;
}