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
    vec3 reflectionDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(reflectionDir, cameraDir), 0.f), material.shininess);
    vec3 specular = vec3(texture(material.specular, texCoords)) * spec * light.specular;

    return ambient + diffuse + specular;
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
        vec3 reflectionDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(reflectionDir, cameraDir), 0.f), material.shininess);
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
    vec3 reflectionDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(reflectionDir, cameraDir), 0.f), material.shininess);
    vec3 specular = vec3(texture(material.specular, texCoords)) * spec * light.specular;

    return intensity * (diffuse + specular);
}

vec3 CalcEnvironment(vec3 normal, vec3 cameraDir)
{
    vec3 reflectionDir = reflect(-cameraDir, normal);
    vec3 sample = vec3(texture(skybox, reflectionDir));
    
    return sample * .5f;
}