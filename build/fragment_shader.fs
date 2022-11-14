#version 330 core
    
struct Material
{
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};

struct Light
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

// in vec2 ourTexCoord;

in vec3 fragWorldPos;
in vec3 normal;
in vec2 texCoords;

out vec4 fragColor;

uniform Material material;
uniform Light light;
uniform Attenuation att;

uniform vec3 cameraPos;

void main()
{
    // Ambient contribution.
    vec3 ambient = vec3(texture(material.diffuse, texCoords)) * light.ambient;
    
    // Diffuse contribution.
    vec3 lightDir = normalize(light.position - fragWorldPos);
    float diff = max(dot(normalize(normal), lightDir), 0.f);
    vec3 diffuse = vec3(texture(material.diffuse, texCoords)) * diff * light.diffuse;
    
    // Specular contribution.
    vec3 reflectionDir = reflect(-lightDir, normal);
    vec3 cameraDir = normalize(cameraPos - fragWorldPos);
    float spec = pow(max(dot(reflectionDir, cameraDir), 0.f), material.shininess);
    vec3 specular = vec3(texture(material.specular, texCoords)) * spec * light.specular;
    
    // Calculate attenuation.
    float d = distance(light.position, fragWorldPos);
    float divisor = 1.f + att.linear * d + att.quadratic * d * d;
    float attVal = 1.f / divisor;
    
    
    vec3 result = attVal * (ambient + diffuse + specular);
    
    fragColor = vec4(result, 1.f);
}