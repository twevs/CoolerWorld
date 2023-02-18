#version 460 core
    
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_ARB_bindless_texture : enable    
	
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
out uvec2 diffuseHandle;
out uvec2 specularHandle;
out uvec2 normalsHandle;
out uvec2 displacementHandle;
out uint objectId;
out uint faceInfo;

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
uniform uint u_objectId;

struct TextureHandle
{
    uvec2 aDiffuseHandle;
    uvec2 aSpecularHandle;
    uvec2 aNormalsHandle;
    uvec2 aDisplacementHandle;
};

#define MAX_MESHES_PER_MODEL 100

layout (std140, binding = 1) uniform TextureHandles
{
	TextureHandle handles[100];
};

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
	
	diffuseHandle = handles[gl_DrawID].aDiffuseHandle;
	specularHandle = handles[gl_DrawID].aSpecularHandle;
	normalsHandle = handles[gl_DrawID].aNormalsHandle;
	displacementHandle = handles[gl_DrawID].aDisplacementHandle;
	
	objectId = u_objectId;
	uint facingX = uint(dot(norm, vec3(1, 0, 0)) + 1);
	uint facingY = uint(dot(norm, vec3(0, 1, 0)) + 1);
	uint facingZ = uint(dot(norm, vec3(0, 0, 1)) + 1);
	faceInfo = (facingX << 4) | (facingY << 2) | facingZ;
}