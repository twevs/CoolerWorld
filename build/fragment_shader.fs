#version 330 core
    
struct Material
{
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

struct Light
{
        vec3 position;
        
        vec3 ambient;
        vec3 diffuse;
        vec3 specular;
};

// in vec2 ourTexCoord;

in vec3 fragWorldPos;
in vec3 normal;

out vec4 fragColor;

uniform Material material;
uniform Light light;

// uniform float mixAlpha;
// uniform sampler2D texture1;
// uniform sampler2D texture2;
uniform vec3 cameraPos;

void main()
{
    // Ambient contribution.
    vec3 ambient = material.ambient * light.ambient;
    
    // Diffuse contribution.
    vec3 lightDir = normalize(light.position - fragWorldPos);
    float diff = max(dot(normalize(normal), lightDir), 0.f);
    vec3 diffuse = material.diffuse * diff * light.diffuse;
    
    // Specular contribution.
    vec3 reflectionDir = reflect(-lightDir, normal);
    vec3 cameraDir = normalize(cameraPos - fragWorldPos);
    float spec = pow(max(dot(reflectionDir, cameraDir), 0.f), material.shininess);
    vec3 specular = material.specular * spec * light.specular;
    
    vec3 result = ambient + diffuse + specular;
    
    fragColor = vec4(result, 1.f);
}