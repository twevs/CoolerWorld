#version 420 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 fragWorldPos;
out vec3 normal;
out vec2 texCoords;
out vec4 fragPosLightSpace;

layout (std140, binding = 0) uniform Matrices
{
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 lightSpaceMatrix;
};
uniform mat4 modelMatrix;
uniform mat3 normalMatrix;

void main()
{
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.f);
    fragWorldPos = vec3(modelMatrix * vec4(aPos, 1.f));
    normal = normalize(normalMatrix * aNormal);
    texCoords = aTexCoords;
	fragPosLightSpace = lightSpaceMatrix * modelMatrix * vec4(aPos, 1.f);
}