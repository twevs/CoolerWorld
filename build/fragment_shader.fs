#version 450 core
    
layout (binding = 10) uniform sampler2D positionBuffer; // Alpha = specular.
layout (binding = 11) uniform sampler2D normalBuffer;   // Alpha reserved for handedness.
layout (binding = 12) uniform sampler2D albedoBuffer;   // Alpha = shininess.
layout (binding = 13) uniform sampler2D ssao;

struct DirLight
{
    vec3 direction;
    
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
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

layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec4 brightColor;

in vec2 texCoords;

layout (std140, binding = 0) uniform Matrices
{
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 dirLightSpaceMatrix;
	mat4 spotLightSpaceMatrix;
    mat4 pointShadowMatrices[6];
};

uniform DirLight dirLight;
vec3 CalcDirLight(DirLight light, vec3 fragPos, vec3 normal, vec3 cameraDir, vec2 inTexCoords, float ao);

uniform SpotLight spotLight;
vec3 CalcSpotLight(SpotLight light, vec3 fragPos, vec3 normal, vec3 cameraDir, vec2 inTexCoords, float ao);

layout (binding = 14) uniform samplerCube skybox;
vec3 CalcEnvironment(vec3 normal, vec3 cameraDir);

uniform bool blinn;

// Shadow maps.
layout (binding = 15) uniform sampler2D dirDepthMap;
layout (binding = 16) uniform sampler2D spotDepthMap;

uniform vec3 cameraPos;

float CalcShadow(vec4 posLightSpace, vec3 nrm, vec3 lightDir, sampler2D depthMap)
{
    posLightSpace.xyz /= posLightSpace.w;
    posLightSpace.xyz += 1.f;
    posLightSpace.xyz /= 2.f;
    float shadowMapDepth = texture(depthMap, posLightSpace.xy).r;
    float bias = max(.05f * (1.f - dot(nrm, lightDir)), .005f);
    if (posLightSpace.z > 1.f)
    {
        return 0.f;
    }
    
    float shadow = 0.f;
    vec2 texelSize = 1.f / textureSize(depthMap, 0);
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float pcfDepth = texture(depthMap, posLightSpace.xy + vec2(x, y) * texelSize).r;
            shadow += (posLightSpace.z - bias > pcfDepth) ? 1.f : 0.f;
        }
    }
    return shadow / 9.f;
}

void main()
{
    vec3 fragPos = texture(positionBuffer, texCoords).rgb;
    vec3 cameraDir = normalize(cameraPos - fragPos);
    float ao = texture(ssao, texCoords).r;
    
    vec3 norm = texture(normalBuffer, texCoords).rgb;
    vec3 dirContribution = CalcDirLight(dirLight, fragPos, norm, cameraDir, texCoords, ao);
    vec3 spotContribution = CalcSpotLight(spotLight, fragPos, norm, cameraDir, texCoords, ao);
    vec3 envContribution = CalcEnvironment(norm, cameraDir);
    
    vec3 result = dirContribution + spotContribution + envContribution;
    
    fragColor = vec4(result, 1.f);
    
    float brightness = dot(fragColor.rgb, vec3(.2126f, .7152f, .0722f));
    if (brightness > 1.f)
    {
        brightColor = fragColor;
    }
    else
    {
        brightColor = vec4(vec3(0.f), 1.f);
    }
}

vec3 CalcDirLight(DirLight light, vec3 fragPos, vec3 normal, vec3 cameraDir, vec2 inTexCoords, float ao)
{
    // Ambient contribution.
    vec3 ambient = texture(albedoBuffer, inTexCoords).rgb * light.ambient * ao;
    
    // Diffuse contribution.
    vec3 lightDir = normalize(-dirLight.direction);
    float diff = max(dot(normal, lightDir), 0.f);
    vec3 diffuse = texture(albedoBuffer, inTexCoords).rgb * diff * light.diffuse;

    // Specular contribution.
    vec3 halfway = normalize(lightDir + cameraDir);
    vec3 reflectionDir = reflect(-lightDir, normal);
    vec3 specVec1 = blinn ? halfway : reflectionDir;
    vec3 specVec2 = blinn ? normal : cameraDir;
    float shininess = texture(albedoBuffer, inTexCoords).a;
    float spec = pow(max(dot(specVec1, specVec2), 0.f), shininess);
    vec3 specular = texture(positionBuffer, inTexCoords).a * spec * light.specular;

	vec4 fragPosDirLightSpace = dirLightSpaceMatrix * vec4(fragPos, 1.f);
    return ambient + (1.f - CalcShadow(fragPosDirLightSpace, normal, lightDir, dirDepthMap)) * (diffuse + specular);
}

vec3 CalcSpotLight(SpotLight light, vec3 fragPos, vec3 normal, vec3 cameraDir, vec2 inTexCoords, float ao)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float dotDirs = dot(lightDir, normalize(-dirLight.direction));
    float intensity = (dotDirs - light.outerCutoff) / (light.innerCutoff - light.outerCutoff);
    intensity = clamp(intensity, 0.f, 1.f);
        
    // Ambient contribution.
    vec3 ambient = texture(albedoBuffer, inTexCoords).rgb * light.ambient * ao;
    
    // Diffuse contribution.
    float diff = max(dot(normal, lightDir), 0.f);
    vec3 diffuse = texture(albedoBuffer, inTexCoords).rgb * diff * light.diffuse;

    // Specular contribution.
    vec3 halfway = normalize(lightDir + cameraDir);
    vec3 reflectionDir = reflect(-lightDir, normal);
    vec3 specVec1 = blinn ? halfway : reflectionDir;
    vec3 specVec2 = blinn ? normal : cameraDir;
    float shininess = texture(albedoBuffer, inTexCoords).a;
    float spec = pow(max(dot(specVec1, specVec2), 0.f), shininess);
    vec3 specular = texture(positionBuffer, inTexCoords).a * spec * light.specular;

	vec4 fragPosSpotLightSpace = spotLightSpaceMatrix * vec4(fragPos, 1.f);
    return intensity * (1.f - CalcShadow(fragPosSpotLightSpace, normal, lightDir, spotDepthMap)) * (diffuse + specular);
}

vec3 CalcEnvironment(vec3 normal, vec3 cameraDir)
{
    vec3 reflectionDir = reflect(-cameraDir, normal);
    vec3 envSample = texture(skybox, reflectionDir).rgb;
    
    return envSample * .1f;
}