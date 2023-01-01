#version 450 core
    
struct Material
{
    sampler2D diffuse;
    sampler2D specular;
    sampler2D normals;
    sampler2D displacement;
    float shininess;
};

layout (location = 0) out vec4 positionBuffer; // Alpha = specular.
layout (location = 1) out vec4 normalBuffer;   // Alpha reserved for handedness.
layout (location = 2) out vec4 albedoBuffer;   // Alpha = shininess.

#define NUM_POINTLIGHTS 4

in vec3 fragPosWS;
in vec2 texCoords;
in mat3 tbn;
in vec3 cameraPosTS;
in vec3 fragPosTS;

uniform Material material;

// Displacement mapping.
uniform bool displace;
uniform float heightScale;

vec2 GetDisplacedTexCoords(vec3 viewDir)
{
    float minLayers = 8.f;
    float maxLayers = 32.f;
    float numLayers = mix(maxLayers, minLayers, max(dot(vec3(0.f, 0.f, 1.f), viewDir), 0.f));
    float layerDepth = 1.f / numLayers;
    float curLayerDepth = 0.f;
    vec2 p = viewDir.xy * heightScale;
    vec2 deltaTexCoords = p / numLayers;
    
    vec2 result = texCoords;
    float curMapDepth = texture(material.displacement, result).r;
    while (curLayerDepth < curMapDepth)
    {
        result -= deltaTexCoords;
        curLayerDepth += layerDepth;
        curMapDepth = texture(material.displacement, result).r;
    }
    
    vec2 prevTexCoords = result + deltaTexCoords;
    
    float prevDist = texture(material.displacement, prevTexCoords).r - (curLayerDepth - layerDepth);
    float curDist = curLayerDepth - curMapDepth;
    
    float weight = prevDist / (curDist + prevDist);
    result = mix(prevTexCoords, result, weight);
    
    return result;
}

void main()
{    
    vec3 cameraDir = normalize(cameraPosTS - fragPosTS);
    vec2 displacedTexCoords = displace ? GetDisplacedTexCoords(cameraDir) : texCoords;
    if (displacedTexCoords.x > 1.f || displacedTexCoords.y > 1.f ||
        displacedTexCoords.x < 0.f || displacedTexCoords.y < 0.f)
    {
        discard;
    }
    vec3 norm = texture(material.normals, displacedTexCoords).rgb;
    norm = normalize(norm * 2.f - 1.f);
    
    positionBuffer.rgb = fragPosWS;
    positionBuffer.a = texture(material.specular, displacedTexCoords).r;
    normalBuffer.rgb = tbn * norm;
    albedoBuffer.rgb = texture(material.diffuse, displacedTexCoords).rgb;
    albedoBuffer.a = material.shininess;
}
