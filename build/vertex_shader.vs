#version 450 core
	
struct DirLight
{
    vec3 direction;
    
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight
{
    vec3 position;
    
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
        
    float linear;
    float quadratic;
};

struct SpotLight
{
    vec3 position;
    vec3 direction;
    
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
        
    float innerCutoff;
    float outerCutoff;
};

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

#define NUM_POINTLIGHTS 4

out vec3 normal;
out vec2 texCoords;
out vec4 fragPosDirLightSpace;
out vec4 fragPosSpotLightSpace;
out vec3 cameraPosTS;
out vec3 fragPosTS;
out vec3 dirLightDirectionTS;
out vec3 pointLightPosTS[NUM_POINTLIGHTS];
out vec3 spotLightPosTS;

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
uniform DirLight dirLight;
uniform PointLight pointLights[NUM_POINTLIGHTS];
uniform SpotLight spotLight;

void main()
{
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.f);
    normal = normalize(normalMatrix * aNormal);
    texCoords = aTexCoords;
	fragPosDirLightSpace = dirLightSpaceMatrix * modelMatrix * vec4(aPos, 1.f);
	fragPosSpotLightSpace = spotLightSpaceMatrix * modelMatrix * vec4(aPos, 1.f);
	
	// NOTE: Assimp's aiProcess_CalcTangentSpace produces unit vectors.
	vec3 tangent = normalize(vec3(modelMatrix * vec4(aTangent, 0.f)));
	vec3 norm = normalize(vec3(modelMatrix * vec4(aNormal, 0.f)));
	tangent = normalize(tangent - dot(tangent, norm) * norm);
	vec3 bitangent = cross(norm, tangent);
	mat3 tbn = transpose(mat3(tangent, bitangent, norm));
	
	cameraPosTS = tbn * cameraPos;
    vec3 fragWorldPos = vec3(modelMatrix * vec4(aPos, 1.f));
	fragPosTS = tbn * fragWorldPos;
	dirLightDirectionTS = tbn * dirLight.direction;
	for (int i = 0; i < NUM_POINTLIGHTS; i++)
	{
		pointLightPosTS[i] = tbn * pointLights[i].position;
	}
	spotLightPosTS = tbn * spotLight.position;
}