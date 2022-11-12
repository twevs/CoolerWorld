#version 330 core

// in vec2 ourTexCoord;

in vec3 fragWorldPos;
in vec3 normal;

out vec4 fragColor;

uniform float ambientStrength;
uniform float diffuseStrength;
uniform float specularStrength;
uniform float shininess;

// uniform float mixAlpha;
// uniform sampler2D texture1;
// uniform sampler2D texture2;
uniform vec3 lightPos;
uniform vec3 cameraPos;
uniform vec3 lightColor;
uniform vec3 objectColor;

void main()
{
    vec3 ambient = ambientStrength * lightColor;
    
    vec3 lightDir = normalize(lightPos - fragWorldPos);
    float diff = max(dot(normalize(normal), lightDir), 0.f);
    vec3 diffuse = diffuseStrength * diff * lightColor;
    
    vec3 reflectionDir = reflect(-lightDir, normal);
    vec3 cameraDir = normalize(cameraPos - fragWorldPos);
    float spec = pow(max(dot(reflectionDir, cameraDir), 0.f), shininess);
    vec3 specular = specularStrength * spec * lightColor;
    
    vec3 result = (ambient + diffuse + specular) * objectColor;
    
    fragColor = vec4(result, 1.f);
}