#version 450 core
	
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

#define NUM_POINTLIGHTS 4

out vec2 texCoords;

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
    gl_Position = mat.projectionMatrix * mat.viewMatrix * modelMatrix * vec4(aPos, 1.f);
    texCoords = aTexCoords;
}