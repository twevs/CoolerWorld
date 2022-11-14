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
        
    float cutoff;
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
    // Ambient contribution.
    vec3 ambient = vec3(texture(material.diffuse, texCoords)) * light.ambient;
    
    vec3 lightDir = normalize(light.position - fragWorldPos);
    float dotDirs = dot(lightDir, -light.direction);
    if (dotDirs > light.cutoff)
    {
        // Diffuse contribution.
        float diff = max(dot(normalize(normal), lightDir), 0.f);
        vec3 diffuse = vec3(texture(material.diffuse, texCoords)) * diff * light.diffuse;
    
        // Specular contribution.
        vec3 reflectionDir = reflect(-lightDir, normal);
        vec3 cameraDir = normalize(cameraPos - fragWorldPos);
        float spec = pow(max(dot(reflectionDir, cameraDir), 0.f), material.shininess);
        vec3 specular = vec3(texture(material.specular, texCoords)) * spec * light.specular;
    
        vec3 result = ambient + diffuse + specular;
        
        fragColor = vec4(result, 1.f);
    }
    else
    {
        fragColor = vec4(ambient, 1.f);
    }
    
    
}