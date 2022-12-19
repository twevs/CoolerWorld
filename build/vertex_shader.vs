#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out vec3 fragWorldPos;
out vec3 normal;
out vec2 texCoords;
out vec4 fragPosDirLightSpace;
out vec4 fragPosSpotLightSpace;
out mat3 tbn;

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
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.f);
    fragWorldPos = vec3(modelMatrix * vec4(aPos, 1.f));
    normal = normalize(normalMatrix * aNormal);
    texCoords = aTexCoords;
	fragPosDirLightSpace = dirLightSpaceMatrix * modelMatrix * vec4(aPos, 1.f);
	fragPosSpotLightSpace = spotLightSpaceMatrix * modelMatrix * vec4(aPos, 1.f);
	
	// NOTE: Assimp's aiProcess_CalcTangentSpace produces unit vectors.
	vec3 tangent = normalize(vec3(modelMatrix * vec4(aTangent, 0.f)));
	vec3 norm = normalize(vec3(modelMatrix * vec4(aNormal, 0.f)));
	tangent = normalize(tangent - dot(tangent, norm) * norm);
	vec3 bitangent = cross(norm, tangent);
	tbn = mat3(tangent, bitangent, norm);
}