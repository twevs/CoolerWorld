#version 330 core

// in vec2 ourTexCoord;

in vec3 fragViewPos;
in vec3 normal;
in vec3 viewLight;

out vec4 fragColor;

uniform float ambientStrength;
uniform float diffuseStrength;
uniform float specularStrength;
uniform float shininess;

uniform vec3 cameraPos;
uniform vec3 lightColor;
uniform vec3 objectColor;

void main()
{
    vec3 ambient = ambientStrength * lightColor;
    
    vec3 lightDir = normalize(viewLight - fragViewPos);
    float diff = max(dot(normalize(normal), lightDir), 0.f);
    vec3 diffuse = diffuseStrength * diff * lightColor;
    
    vec3 reflectionDir = reflect(-lightDir, normal);
    vec3 cameraDir = normalize(-fragViewPos);
    float spec = pow(max(dot(reflectionDir, cameraDir), 0.f), shininess);
    vec3 specular = specularStrength * spec * lightColor;
    
    vec3 result = (ambient + diffuse + specular) * objectColor;
    
    fragColor = vec4(result, 1.f);
}