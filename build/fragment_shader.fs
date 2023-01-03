#version 450 core
    
struct GBuffer
{
    sampler2D position; // Alpha = specular.
    sampler2D normal;   // Alpha reserved for handedness.
    sampler2D albedo;   // Alpha = shininess.
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

layout (std140, binding = 0) uniform Matrices
{
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 dirLightSpaceMatrix;
	mat4 spotLightSpaceMatrix;
    mat4 pointShadowMatrices[6];
};

uniform GBuffer gBuffer;

uniform DirLight dirLight;
vec3 CalcDirLight(DirLight light, vec3 fragPos, vec3 normal, vec3 cameraDir, vec2 inTexCoords);

uniform PointLight pointLights[NUM_POINTLIGHTS];
vec3 CalcPointLights(PointLight[NUM_POINTLIGHTS] lights, vec3 fragPos, vec3 normal, vec3 cameraDir, vec2 inTexCoords);

uniform SpotLight spotLight;
vec3 CalcSpotLight(SpotLight light, vec3 fragPos, vec3 normal, vec3 cameraDir, vec2 inTexCoords);

uniform samplerCube skybox;
vec3 CalcEnvironment(vec3 normal, vec3 cameraDir);

uniform bool blinn;

// Shadow maps.
uniform sampler2D dirDepthMap;
uniform sampler2D spotDepthMap;
uniform samplerCube pointDepthMaps[NUM_POINTLIGHTS];

// Point light shadow mapping.
uniform float pointFar;

// Displacement mapping.
uniform bool displace;
uniform float heightScale;

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

float CalcPointShadow(vec3 nrm, vec3 fragPos, vec3 lightPos, samplerCube depthMap)
{
    vec3 lightToFrag = fragPos - lightPos;
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
    float viewDistance = distance(cameraPos, fragPos);
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
    vec3 fragPos = texture(gBuffer.position, texCoords).rgb;
    vec3 cameraDir = normalize(cameraPos - fragPos);
    
    vec3 norm = texture(gBuffer.normal, texCoords).rgb;
    vec3 dirContribution = CalcDirLight(dirLight, fragPos, norm, cameraDir, texCoords);
    vec3 pointsContribution = CalcPointLights(pointLights, fragPos, norm, cameraDir, texCoords);
    vec3 spotContribution = CalcSpotLight(spotLight, fragPos, norm, cameraDir, texCoords);
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

vec3 CalcDirLight(DirLight light, vec3 fragPos, vec3 normal, vec3 cameraDir, vec2 inTexCoords)
{
    // Ambient contribution.
    vec3 ambient = texture(gBuffer.albedo, inTexCoords).rgb * light.ambient;
    
    // Diffuse contribution.
    vec3 lightDir = normalize(-dirLight.direction);
    float diff = max(dot(normal, lightDir), 0.f);
    vec3 diffuse = texture(gBuffer.albedo, inTexCoords).rgb * diff * light.diffuse;

    // Specular contribution.
    vec3 halfway = normalize(lightDir + cameraDir);
    vec3 reflectionDir = reflect(-lightDir, normal);
    vec3 specVec1 = blinn ? halfway : reflectionDir;
    vec3 specVec2 = blinn ? normal : cameraDir;
    float shininess = texture(gBuffer.albedo, inTexCoords).a;
    float spec = pow(max(dot(specVec1, specVec2), 0.f), shininess);
    vec3 specular = texture(gBuffer.position, inTexCoords).a * spec * light.specular;

	vec4 fragPosDirLightSpace = dirLightSpaceMatrix * vec4(fragPos, 1.f);
    return ambient + (1.f - CalcShadow(fragPosDirLightSpace, normal, lightDir, dirDepthMap)) * (diffuse + specular);
}

vec3 CalcPointLights(PointLight[NUM_POINTLIGHTS] lights, vec3 fragPos, vec3 normal, vec3 cameraDir, vec2 inTexCoords)
{
    vec3 result = vec3(0.f);
    
    for (int i = 0; i < lights.length(); i++)
    {
        PointLight light = lights[i];
        vec3 lightPos = light.position;
        
        float d = distance(fragPos, lightPos);
        float intensity = 1.f / (1.f + light.linear * d + light.quadratic * d * d);
        
        // Ambient contribution.
        vec3 ambient = texture(gBuffer.albedo, inTexCoords).rgb * light.ambient;
    
        // Diffuse contribution.
        vec3 lightDir = normalize(lightPos - fragPos);
        float diff = max(dot(normal, lightDir), 0.f);
        vec3 diffuse = texture(gBuffer.albedo, inTexCoords).rgb * diff * light.diffuse;

        // Specular contribution.
        vec3 halfway = normalize(lightDir + cameraDir);
        vec3 reflectionDir = reflect(-lightDir, normal);
        vec3 specVec1 = blinn ? halfway : reflectionDir;
        vec3 specVec2 = blinn ? normal : cameraDir;
        float shininess = texture(gBuffer.albedo, inTexCoords).a;
        float spec = pow(max(dot(specVec1, specVec2), 0.f), shininess);
        vec3 specular = texture(gBuffer.position, inTexCoords).a * spec * light.specular;
        
        float shadow = CalcPointShadow(normal, fragPos, lights[i].position, pointDepthMaps[i]);
        result += intensity * (ambient + (1.f - shadow) * (diffuse + specular));
    }
    
    return result;
}

vec3 CalcSpotLight(SpotLight light, vec3 fragPos, vec3 normal, vec3 cameraDir, vec2 inTexCoords)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float dotDirs = dot(lightDir, normalize(-dirLight.direction));
    float intensity = (dotDirs - light.outerCutoff) / (light.innerCutoff - light.outerCutoff);
    intensity = clamp(intensity, 0.f, 1.f);
        
    // Ambient contribution.
    vec3 ambient = texture(gBuffer.albedo, inTexCoords).rgb * light.ambient;
    
    // Diffuse contribution.
    float diff = max(dot(normal, lightDir), 0.f);
    vec3 diffuse = texture(gBuffer.albedo, inTexCoords).rgb * diff * light.diffuse;

    // Specular contribution.
    vec3 halfway = normalize(lightDir + cameraDir);
    vec3 reflectionDir = reflect(-lightDir, normal);
    vec3 specVec1 = blinn ? halfway : reflectionDir;
    vec3 specVec2 = blinn ? normal : cameraDir;
    float shininess = texture(gBuffer.albedo, inTexCoords).a;
    float spec = pow(max(dot(specVec1, specVec2), 0.f), shininess);
    vec3 specular = texture(gBuffer.position, inTexCoords).a * spec * light.specular;

	vec4 fragPosSpotLightSpace = spotLightSpaceMatrix * vec4(fragPos, 1.f);
    return intensity * (1.f - CalcShadow(fragPosSpotLightSpace, normal, lightDir, spotDepthMap)) * (diffuse + specular);
}

vec3 CalcEnvironment(vec3 normal, vec3 cameraDir)
{
    vec3 reflectionDir = reflect(-cameraDir, normal);
    vec3 envSample = texture(skybox, reflectionDir).rgb;
    
    return envSample * .1f;
}