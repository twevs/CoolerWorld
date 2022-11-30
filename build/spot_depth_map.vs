#version 420 core

layout (location = 0) in vec3 aPos;

layout (std140, binding = 0) uniform Matrices
{
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 dirLightSpaceMatrix;
	mat4 spotLightSpaceMatrix;
};
uniform mat4 modelMatrix;
uniform mat3 normalMatrix;

void main()
{
    gl_Position = spotLightSpaceMatrix * modelMatrix * vec4(aPos, 1.f);
}