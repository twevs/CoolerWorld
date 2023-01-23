#version 420 core

layout (location = 0) in vec3 aPos;

struct Matrices
{
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 dirLightSpaceMatrix;
	mat4 spotLightSpaceMatrix;
    mat4 pointShadowMatrices[6];
};
layout (std140, binding = 0) uniform MatricesArray
{
	Matrices matrices[3];
};
uniform int matricesIndex;
uniform mat4 modelMatrix;
uniform mat3 normalMatrix;

void main()
{
	Matrices mat = matrices[matricesIndex];
    gl_Position = mat.spotLightSpaceMatrix * modelMatrix * vec4(aPos, 1.f);
}