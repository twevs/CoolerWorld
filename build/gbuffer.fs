#version 450 core
    
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_ARB_bindless_texture : enable    
    
layout (binding = 10) uniform sampler2D diffuseTex;
layout (binding = 11) uniform sampler2D specularTex;
layout (binding = 12) uniform sampler2D normalsTex;
layout (binding = 13) uniform sampler2D displacementTex;
uniform float shininess;

layout (location = 0) out vec4 positionBuffer; // Alpha = specular.
layout (location = 1) out vec4 normalBuffer;   // Alpha reserved for handedness.
layout (location = 2) out vec4 albedoBuffer;   // Alpha = shininess.
layout (location = 3) out uvec2 pickingBuffer;

#define NUM_POINTLIGHTS 4

in vec3 fragPosWS;
in vec2 texCoords;
in mat3 tbn;
in vec3 cameraPosTS;
in vec3 fragPosTS;
in flat uvec2 diffuseHandle;
in flat uvec2 specularHandle;
in flat uvec2 normalsHandle;
in flat uvec2 displacementHandle;
in flat uint objectId;
in flat uint faceInfo;

// Displacement mapping.
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
    
    sampler2D displacement = sampler2D(displacementHandle);
    
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
    sampler2D diffuse = sampler2D(diffuseHandle);
    sampler2D specular = sampler2D(specularHandle);
    sampler2D normals = sampler2D(normalsHandle);
    
    vec3 cameraDir = normalize(cameraPosTS - fragPosTS);
    vec2 displacedTexCoords = all(equal(displacementHandle, uvec2(0))) ? texCoords : GetDisplacedTexCoords(cameraDir);
    if (any(lessThan(displacedTexCoords, vec2(0.f)))
        || any(greaterThan(displacedTexCoords, vec2(1.f))))
    {
        discard;
    }
    vec3 norm = texture(normals, displacedTexCoords).rgb;
    norm = normalize(norm * 2.f - 1.f);
    
    positionBuffer.rgb = fragPosWS;
    positionBuffer.a = texture(specular, displacedTexCoords).r;
    normalBuffer.rgb = tbn * norm;
    albedoBuffer.rgb = texture(diffuse, displacedTexCoords).rgb;
    albedoBuffer.a = shininess;
    pickingBuffer.r = objectId;
    pickingBuffer.g = faceInfo;
}
