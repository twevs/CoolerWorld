#version 450 core
    
struct Material
{
    float shininess;
};

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

layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec4 brightColor;

#define NUM_POINTLIGHTS 4

in vec2 texCoords;
in vec4 fragPosDirLightSpace;
in vec4 fragPosSpotLightSpace;
in vec3 cameraPosTS;
in vec3 fragPosTS;
in vec3 dirLightDirectionTS;
in vec3 pointLightPosTS[NUM_POINTLIGHTS];
in vec3 spotLightPosTS;
in vec3 normalWS;
in vec3 fragWorldPos;

uniform Material material;
layout (binding = 10) uniform sampler2D diffuseTex;
layout (binding = 11) uniform sampler2D specularTex;
layout (binding = 12) uniform sampler2D normalsTex;
layout (binding = 13) uniform sampler2D displacementTex;

uniform DirLight dirLight;
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 cameraDir, vec2 inTexCoords);

uniform PointLight pointLights[NUM_POINTLIGHTS];
vec3 CalcPointLights(PointLight[NUM_POINTLIGHTS] lights, vec3 normal, vec3 cameraDir, vec2 inTexCoords);

uniform SpotLight spotLight;
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 cameraDir, vec2 inTexCoords);

layout (binding = 14) uniform samplerCube skybox;
vec3 CalcEnvironment(vec3 normal, vec3 cameraDir);

uniform bool blinn;

// Shadow maps.
layout (binding = 15) uniform sampler2D dirDepthMap;
layout (binding = 16) uniform sampler2D spotDepthMap;
layout (binding = 17) uniform samplerCube pointDepthMaps[NUM_POINTLIGHTS];

// Point light shadow mapping.
uniform float pointFar;

// Displacement mapping.
uniform bool displace;
uniform float heightScale;

vec2 GetDisplacedTexCoords(vec3 viewDir)
{
    float minLayers = 8.f;
    float maxLayers = 32.f;
    float numLayers = mix(maxLayers, minLayers, max(dot(vec3(0.f, 0.f, 1.f), viewDir), 0.f));
    float layerDepth = 1.f / numLayers;
    float curLayerDepth = 0.f;
    vec2 p = viewDir.xy * heightScale;
    vec2 deltaTexCoords = p / numLayers;
    
    vec2 result = texCoords;
    float curMapDepth = texture(displacementTex, result).r;
    while (curLayerDepth < curMapDepth)
    {
        result -= deltaTexCoords;
        curLayerDepth += layerDepth;
        curMapDepth = texture(displacementTex, result).r;
    }
    
    vec2 prevTexCoords = result + deltaTexCoords;
    
    float prevDist = texture(displacementTex, prevTexCoords).r - (curLayerDepth - layerDepth);
    float curDist = curLayerDepth - curMapDepth;
    
    float weight = prevDist / (curDist + prevDist);
    result = mix(prevTexCoords, result, weight);
    
    return result;
}

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

float CalcPointShadow(vec3 nrm, vec3 lightPos, samplerCube depthMap)
{
    vec3 lightToFrag = fragWorldPos - lightPos;
    float shadowMapDepth = texture(depthMap, lightToFrag).r * pointFar;
    float bias = max(.05f * (1.f - dot(nrm, normalize(lightToFrag))), .005f);
    float dist = length(lightToFrag);
    // return dist - bias > shadowMapDepth ? 1.f : 0.f;
    
    vec3 sampleOffsetDirections[20] =
    {
        vec3( 1.f,  1.f,  1.f), vec3( 1.f, -1.f,  1.f), vec3(-1.f, -1.f,  1.f), vec3(-1.f,  1.f,  1.f),
        vec3( 1.f,  1.f, -1.f), vec3( 1.f, -1.f, -1.f), vec3(-1.f, -1.f, -1.f), vec3(-1.f,  1.f, -1.f), 
        vec3( 1.f,  1.f,  0.f), vec3( 1.f, -1.f,  0.f), vec3(-1.f, -1.f,  0.f), vec3(-1.f,  1.f,  0.f),
        vec3( 1.f,  0.f,  1.f), vec3(-1.f,  0.f,  1.f), vec3( 1.f,  0.f, -1.f), vec3(-1.f,  0.f, -1.f),
        vec3( 0.f,  1.f,  1.f), vec3( 0.f, -1.f,  1.f), vec3( 0.f, -1.f, -1.f), vec3( 0.f,  1.f, -1.f),
    };
    
    float shadow = 0.f;
    float viewDistance = distance(cameraPosTS, fragPosTS);
    float cubeSize = (1.f + (viewDistance / pointFar)) / 25.f;
    for (int i = 0; i < 20; i++)
    {
        float pcfDepth = texture(depthMap, lightToFrag + sampleOffsetDirections[i] * cubeSize).r * pointFar;
        shadow += dist - bias > pcfDepth ? 1.f : 0.f;
    }
    return shadow / 20.f;
}

void main()
{
    vec3 cameraDir = normalize(cameraPosTS - fragPosTS);
    vec2 displacedTexCoords = displace ? GetDisplacedTexCoords(cameraDir) : texCoords;
    if (displacedTexCoords.x > 1.f || displacedTexCoords.y > 1.f ||
        displacedTexCoords.x < 0.f || displacedTexCoords.y < 0.f)
    {
        discard;
    }
    vec3 norm = texture(normalsTex, displacedTexCoords).rgb;
    norm = normalize(norm * 2.f - 1.f);
    vec3 dirContribution = CalcDirLight(dirLight, norm, cameraDir, displacedTexCoords);
    vec3 pointsContribution = CalcPointLights(pointLights, norm, cameraDir, displacedTexCoords);
    vec3 spotContribution = CalcSpotLight(spotLight, norm, cameraDir, displacedTexCoords);
    vec3 envContribution = CalcEnvironment(norm, cameraDir);
    
    vec3 result = dirContribution + pointsContribution + spotContribution + envContribution;
    
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

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 cameraDir, vec2 inTexCoords)
{
    // Ambient contribution.
    vec3 ambient = vec3(texture(diffuseTex, inTexCoords)) * light.ambient;
    
    // Diffuse contribution.
    vec3 lightDir = normalize(-dirLightDirectionTS);
    float diff = max(dot(normal, lightDir), 0.f);
    vec3 diffuse = vec3(texture(diffuseTex, inTexCoords)) * diff * light.diffuse;

    // Specular contribution.
    vec3 halfway = normalize(lightDir + cameraDir);
    vec3 reflectionDir = reflect(-lightDir, normal);
    vec3 specVec1 = blinn ? halfway : reflectionDir;
    vec3 specVec2 = blinn ? normal : cameraDir;
    float spec = pow(max(dot(specVec1, specVec2), 0.f), material.shininess);
    vec3 specular = vec3(texture(specularTex, inTexCoords)) * spec * light.specular;

    return ambient + (1.f - CalcShadow(fragPosDirLightSpace, normal, lightDir, dirDepthMap)) * (diffuse + specular);
}

vec3 CalcPointLights(PointLight[NUM_POINTLIGHTS] lights, vec3 normal, vec3 cameraDir, vec2 inTexCoords)
{
    vec3 result = vec3(0.f);
    
    for (int i = 0; i < lights.length(); i++)
    {
        PointLight light = lights[i];
        vec3 lightPosTS = pointLightPosTS[i];
        
        float d = distance(fragPosTS, lightPosTS);
        float intensity = 1.f / (1.f + light.linear * d + light.quadratic * d * d);
        
        // Ambient contribution.
        vec3 ambient = vec3(texture(diffuseTex, inTexCoords)) * light.ambient;
    
        // Diffuse contribution.
        vec3 lightDir = normalize(lightPosTS - fragPosTS);
        float diff = max(dot(normal, lightDir), 0.f);
        vec3 diffuse = vec3(texture(diffuseTex, inTexCoords)) * diff * light.diffuse;

        // Specular contribution.
        vec3 halfway = normalize(lightDir + cameraDir);
        vec3 reflectionDir = reflect(-lightDir, normal);
        vec3 specVec1 = blinn ? halfway : reflectionDir;
        vec3 specVec2 = blinn ? normal : cameraDir;
        float spec = pow(max(dot(specVec1, specVec2), 0.f), material.shininess);
        vec3 specular = vec3(texture(specularTex, inTexCoords)) * spec * light.specular;
        
        float shadow = CalcPointShadow(normalWS, lights[i].position, pointDepthMaps[i]);
        result += intensity * (ambient + (1.f - shadow) * (diffuse + specular));
    }
    
    return result;
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 cameraDir, vec2 inTexCoords)
{
    vec3 lightDir = normalize(spotLightPosTS - fragPosTS);
    float dotDirs = dot(lightDir, normalize(-dirLightDirectionTS));
    float intensity = (dotDirs - light.outerCutoff) / (light.innerCutoff - light.outerCutoff);
    intensity = clamp(intensity, 0.f, 1.f);
        
    // Ambient contribution.
    vec3 ambient = vec3(texture(diffuseTex, inTexCoords)) * light.ambient;
    
    // Diffuse contribution.
    float diff = max(dot(normal, lightDir), 0.f);
    vec3 diffuse = vec3(texture(diffuseTex, inTexCoords)) * diff * light.diffuse;

    // Specular contribution.
    vec3 halfway = normalize(lightDir + cameraDir);
    vec3 reflectionDir = reflect(-lightDir, normal);
    vec3 specVec1 = blinn ? halfway : reflectionDir;
    vec3 specVec2 = blinn ? normal : cameraDir;
    float spec = pow(max(dot(specVec1, specVec2), 0.f), material.shininess);
    vec3 specular = vec3(texture(specularTex, inTexCoords)) * spec * light.specular;

    return intensity * (1.f - CalcShadow(fragPosSpotLightSpace, normal, lightDir, spotDepthMap)) * (diffuse + specular);
}

vec3 CalcEnvironment(vec3 normal, vec3 cameraDir)
{
    vec3 reflectionDir = reflect(-cameraDir, normal);
    vec3 envSample = vec3(texture(skybox, reflectionDir));
    
    return envSample * .2f;
}