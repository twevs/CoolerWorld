#version 450 core
    
layout (binding = 10) uniform sampler2D diffuse;
layout (binding = 11) uniform sampler2D specular;
layout (binding = 12) uniform sampler2D normals;
layout (binding = 13) uniform sampler2D displacement;
uniform float shininess;

layout (location = 0) out vec4 positionBuffer; // Alpha = specular.
layout (location = 1) out vec4 normalBuffer;   // Alpha reserved for handedness.
layout (location = 2) out vec4 albedoBuffer;   // Alpha = shininess.

#define NUM_POINTLIGHTS 4

in vec3 fragPosVS;
in vec2 texCoords;
in mat3 tbn;
in vec3 cameraPosTS;
in vec3 fragPosTS;

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
    float curMapDepth = texture(displacement, result).r;
    while (curLayerDepth < curMapDepth)
    {
        result -= deltaTexCoords;
        curLayerDepth += layerDepth;
        curMapDepth = texture(displacement, result).r;
    }
    
    vec2 prevTexCoords = result + deltaTexCoords;
    
    float prevDist = texture(displacement, prevTexCoords).r - (curLayerDepth - layerDepth);
    float curDist = curLayerDepth - curMapDepth;
    
    float weight = prevDist / (curDist + prevDist);
    result = mix(prevTexCoords, result, weight);
    
    return result;
}

void main()
{    
    vec3 cameraDir = normalize(cameraPosTS - fragPosTS);
    vec2 displacedTexCoords = displace ? GetDisplacedTexCoords(cameraDir) : texCoords;
    if (any(lessThan(displacedTexCoords, vec2(0.f)))
        || any(greaterThan(displacedTexCoords, vec2(1.f))))
    {
        discard;
    }
    vec3 norm = texture(normals, displacedTexCoords).rgb;
    norm = normalize(norm * 2.f - 1.f);
    
    positionBuffer.rgb = fragPosVS;
    positionBuffer.a = texture(specular, displacedTexCoords).r;
    normalBuffer.rgb = tbn * norm;
    albedoBuffer.rgb = texture(diffuse, displacedTexCoords).rgb;
    albedoBuffer.a = shininess;
}
