#version 330 core
    
struct Material
{
    sampler2D diffuse;
    sampler2D specular;
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

in vec3 fragWorldPos;
in vec3 normal;
in vec2 texCoords;
in vec4 fragPosLightSpace;

out vec4 fragColor;

uniform Material material;

uniform DirLight dirLight;
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 cameraDir);

#define NUM_POINTLIGHTS 4
uniform PointLight pointLights[NUM_POINTLIGHTS];
vec3 CalcPointLights(PointLight[NUM_POINTLIGHTS] lights, vec3 normal, vec3 cameraDir);

uniform SpotLight spotLight;
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 cameraDir);

uniform vec3 cameraPos;

uniform samplerCube skybox;
vec3 CalcEnvironment(vec3 normal, vec3 cameraDir);

uniform bool blinn;

uniform sampler2D depthMap;

float CalcShadow(vec3 nrm, vec3 lightDir)
{
    vec4 posLightSpace = fragPosLightSpace;
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
            shadow += posLightSpace.z - bias > pcfDepth ? 1.f : 0.f;
        }
    }
    return shadow / 9.f;
}

void main()
{
    vec3 norm = normalize(normal);
    vec3 cameraDir = normalize(cameraPos - fragWorldPos);
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
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.f);
    vec3 diffuse = vec3(texture(material.diffuse, texCoords)) * diff * light.diffuse;

    // Specular contribution.
    vec3 halfway = normalize(lightDir + cameraDir);
    vec3 reflectionDir = reflect(-lightDir, normal);
    vec3 specVec1 = blinn ? halfway : reflectionDir;
    vec3 specVec2 = blinn ? normal : cameraDir;
    float spec = pow(max(dot(specVec1, specVec2), 0.f), material.shininess);
    vec3 specular = vec3(texture(material.specular, texCoords)) * spec * light.specular;

    return ambient + (1.f - CalcShadow(normal, lightDir)) * (diffuse + specular);
}

vec3 CalcPointLights(PointLight[NUM_POINTLIGHTS] lights, vec3 normal, vec3 cameraDir)
{
    vec3 result = vec3(0.f);
    
    for (int i = 0; i < lights.length(); i++)
    {
        PointLight light = lights[i];
        
        float d = distance(fragWorldPos, light.position);
        float intensity = 1.f / (1.f + light.linear * d + light.quadratic * d * d);
        
        // Ambient contribution.
        vec3 ambient = vec3(texture(material.diffuse, texCoords)) * light.ambient;
    
        // Diffuse contribution.
        vec3 lightDir = normalize(light.position - fragWorldPos);
        float diff = max(dot(normal, lightDir), 0.f);
        vec3 diffuse = vec3(texture(material.diffuse, texCoords)) * diff * light.diffuse;

        // Specular contribution.
        vec3 halfway = normalize(lightDir + cameraDir);
        vec3 reflectionDir = reflect(-lightDir, normal);
        vec3 specVec1 = blinn ? halfway : reflectionDir;
        vec3 specVec2 = blinn ? normal : cameraDir;
        float spec = pow(max(dot(specVec1, specVec2), 0.f), material.shininess);
        vec3 specular = vec3(texture(material.specular, texCoords)) * spec * light.specular;
        
        result += intensity * (ambient + diffuse + specular);
    }

    return result;
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 cameraDir)
{
    vec3 lightDir = normalize(light.position - fragWorldPos);
    float dotDirs = dot(lightDir, normalize(-light.direction));
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

    return intensity * (diffuse + specular);
}

vec3 CalcEnvironment(vec3 normal, vec3 cameraDir)
{
    vec3 reflectionDir = reflect(-cameraDir, normal);
    vec3 sample = vec3(texture(skybox, reflectionDir));
    
    return sample * .2f;
}