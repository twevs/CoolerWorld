#version 450 core
	
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out vec3 fragPosWS;
out vec2 texCoords;
out mat3 tbn;
out vec3 cameraPosTS;
out vec3 fragPosTS;

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
uniform vec3 cameraPos;

void main()
{
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.f);
    fragPosWS = vec3(modelMatrix * vec4(aPos, 1.f));
    texCoords = aTexCoords;
	
	// NOTE: Assimp's aiProcess_CalcTangentSpace produces unit vectors.
	vec3 tangent = normalize(normalMatrix * aTangent);
	vec3 norm = normalize(normalMatrix * aNormal);
	tangent = normalize(tangent - dot(tangent, norm) * norm);
	// vec3 bitangent = normalize(vec3(modelMatrix * vec4(aBitangent, 0.f)));
	vec3 bitangent = cross(norm, tangent);
	tbn = mat3(tangent, bitangent, norm);
	
	mat3 invTbn = transpose(tbn);
	cameraPosTS = invTbn * cameraPos;
	fragPosTS = invTbn * fragPosWS;
}