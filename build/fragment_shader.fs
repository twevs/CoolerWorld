#version 450 core
    
struct Material
{
    sampler2D diffuse;
    sampler2D specular;
    sampler2D normals;
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

out vec4 fragColor;

uniform Material material;

uniform DirLight dirLight;
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 cameraDir);

uniform PointLight pointLights[NUM_POINTLIGHTS];
vec3 CalcPointLights(PointLight[NUM_POINTLIGHTS] lights, vec3 normal, vec3 cameraDir);

uniform SpotLight spotLight;
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 cameraDir);

uniform samplerCube skybox;
vec3 CalcEnvironment(vec3 normal, vec3 cameraDir);

uniform bool blinn;

uniform sampler2D dirDepthMap;
uniform sampler2D spotDepthMap;
uniform samplerCube pointDepthMaps[NUM_POINTLIGHTS];

uniform float pointFar;

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
    vec3 norm = texture(material.normals, texCoords).rgb;
    norm = normalize(norm * 2.f - 1.f);
    vec3 cameraDir = normalize(cameraPosTS - fragPosTS);
    vec3 dirContribution = CalcDirLight(dirLight, norm, cameraDir);
    vec3 pointsContribution = CalcPointLights(pointLights, norm, cameraDir);
    vec3 spotContribution = CalcSpotLight(spotLight, norm, cameraDir);
    vec3 envContribution = CalcEnvironment(norm, cameraDir);
    
    vec3 result = dirContribution + pointsContribution + spotContribution + envContribution;
    
    fragColor = vec4(result, 1.f);
}

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 cameraDir)
{
    // Ambient contribution.
    vec3 ambient = vec3(texture(material.diffuse, texCoords)) * light.ambient;
    
    // Diffuse contribution.
    vec3 lightDir = normalize(-dirLightDirectionTS);
    float diff = max(dot(normal, lightDir), 0.f);
    vec3 diffuse = vec3(texture(material.diffuse, texCoords)) * diff * light.diffuse;

    // Specular contribution.
    vec3 halfway = normalize(lightDir + cameraDir);
    vec3 reflectionDir = reflect(-lightDir, normal);
    vec3 specVec1 = blinn ? halfway : reflectionDir;
    vec3 specVec2 = blinn ? normal : cameraDir;
    float spec = pow(max(dot(specVec1, specVec2), 0.f), material.shininess);
    vec3 specular = vec3(texture(material.specular, texCoords)) * spec * light.specular;

    return ambient + (1.f - CalcShadow(fragPosDirLightSpace, normal, lightDir, dirDepthMap)) * (diffuse + specular);
}

vec3 CalcPointLights(PointLight[NUM_POINTLIGHTS] lights, vec3 normal, vec3 cameraDir)
{
    vec3 result = vec3(0.f);
    
    for (int i = 0; i < lights.length(); i++)
    {
        PointLight light = lights[i];
        vec3 lightPosTS = pointLightPosTS[i];
        
        float d = distance(fragPosTS, lightPosTS);
        float intensity = 1.f / (1.f + light.linear * d + light.quadratic * d * d);
        
        // Ambient contribution.
        vec3 ambient = vec3(texture(material.diffuse, texCoords)) * light.ambient;
    
        // Diffuse contribution.
        vec3 lightDir = normalize(lightPosTS - fragPosTS);
        float diff = max(dot(normal, lightDir), 0.f);
        vec3 diffuse = vec3(texture(material.diffuse, texCoords)) * diff * light.diffuse;

        // Specular contribution.
        vec3 halfway = normalize(lightDir + cameraDir);
        vec3 reflectionDir = reflect(-lightDir, normal);
        vec3 specVec1 = blinn ? halfway : reflectionDir;
        vec3 specVec2 = blinn ? normal : cameraDir;
        float spec = pow(max(dot(specVec1, specVec2), 0.f), material.shininess);
        vec3 specular = vec3(texture(material.specular, texCoords)) * spec * light.specular;
        
        float shadow = CalcPointShadow(normalWS, lights[i].position, pointDepthMaps[i]);
        result += intensity * (ambient + (1.f - shadow) * (diffuse + specular));
    }
    
    return result;
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 cameraDir)
{
    vec3 lightDir = normalize(spotLightPosTS - fragPosTS);
    float dotDirs = dot(lightDir, normalize(-dirLightDirectionTS));
    float intensity = (dotDirs - light.outerCutoff) / (light.innerCutoff - light.outerCutoff);
    intensity = clamp(intensity, 0.f, 1.f);
        
    // Ambient contribution.
    vec3 ambient = vec3(texture(material.diffuse, texCoords)) * light.ambient;
    
    // Diffuse contribution.
    float diff = max(dot(normal, lightDir), 0.f);
    vec3 diffuse = vec3(texture(material.diffuse, texCoords)) * diff * light.diffuse;

    // Specular contribution.
    vec3 halfway = normalize(lightDir + cameraDir);
    vec3 reflectionDir = reflect(-lightDir, normal);
    vec3 specVec1 = blinn ? halfway : reflectionDir;
    vec3 specVec2 = blinn ? normal : cameraDir;
    float spec = pow(max(dot(specVec1, specVec2), 0.f), material.shininess);
    vec3 specular = vec3(texture(material.specular, texCoords)) * spec * light.specular;

    return intensity * (1.f - CalcShadow(fragPosSpotLightSpace, normal, lightDir, spotDepthMap)) * (diffuse + specular);
}

vec3 CalcEnvironment(vec3 normal, vec3 cameraDir)
{
    vec3 reflectionDir = reflect(-cameraDir, normal);
    vec3 envSample = vec3(texture(skybox, reflectionDir));
    
    return envSample * .2f;
}