#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out VS_OUT
{
	vec3 vs_Normal;
} vs_out;

layout (std140, binding = 0) uniform Matrices
{
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 dirLightSpaceMatrix;
	mat4 spotLightSpaceMatrix;
    mat4 pointShadowMatrices[6];
};
uniform mat4 modelMatrix;
uniform mat3 normalMatrix;

void main()
{
    gl_Position = viewMatrix * modelMatrix * vec4(aPos, 1.f);
	mat3 viewNormalMatrix = mat3(transpose(inverse(viewMatrix * modelMatrix)));
	vs_out.vs_Normal = normalize(viewNormalMatrix * aNormal);
}