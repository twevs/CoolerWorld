#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 fragViewPos;
out vec3 normal;
out vec3 color;

uniform mat4 modelMatrix;
uniform mat3 normalMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

uniform float ambientStrength;
uniform float diffuseStrength;
uniform float specularStrength;
uniform float shininess;

uniform vec3 lightPos;
uniform vec3 cameraPos;
uniform vec3 lightColor;
uniform vec3 objectColor;

void main()
{
    vec3 fragViewPos = vec3(viewMatrix * modelMatrix * vec4(aPos, 1.f));
    vec3 normal = mat3(viewMatrix) * normalMatrix * aNormal;
    
    vec3 ambient = ambientStrength * lightColor;
    
    vec3 lightDir = normalize(vec3(viewMatrix * vec4(lightPos, 1.f)) - fragViewPos);
    float diff = max(dot(normalize(normal), lightDir), 0.f);
    vec3 diffuse = diffuseStrength * diff * lightColor;
    
    vec3 reflectionDir = reflect(-lightDir, normal);
    vec3 cameraDir = normalize(vec3(viewMatrix * vec4(cameraPos, 1.f)) - fragViewPos);
    float spec = pow(max(dot(reflectionDir, cameraDir), 0.f), shininess);
    vec3 specular = specularStrength * spec * lightColor;
    
    color = (ambient + diffuse + specular) * objectColor;
    
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.f);
}