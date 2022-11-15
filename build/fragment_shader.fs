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
};

struct Attenuation
{
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
uniform SpotLight light;

uniform vec3 cameraPos;

void main()
{
    vec3 lightDir = normalize(light.position - fragWorldPos);
    float dotDirs = dot(lightDir, normalize(-light.direction));
    float intensity = (dotDirs - light.outerCutoff) / (light.innerCutoff - light.outerCutoff);
    intensity = clamp(intensity, 0.f, 1.f);
        
    // Ambient contribution.
    vec3 ambient = vec3(texture(material.diffuse, texCoords)) * light.ambient;
    
    // Diffuse contribution.
    float diff = max(dot(normalize(normal), lightDir), 0.f);
    vec3 diffuse = vec3(texture(material.diffuse, texCoords)) * diff * light.diffuse;

    // Specular contribution.
    vec3 reflectionDir = reflect(-lightDir, normal);
    vec3 cameraDir = normalize(cameraPos - fragWorldPos);
    float spec = pow(max(dot(reflectionDir, cameraDir), 0.f), material.shininess);
    vec3 specular = vec3(texture(material.specular, texCoords)) * spec * light.specular;

    vec3 result = ambient + intensity * (diffuse + specular);
    
    fragColor = vec4(result, 1.f);
}