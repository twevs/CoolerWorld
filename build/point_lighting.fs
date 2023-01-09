#version 450 core
    
layout (binding = 10) uniform sampler2D positionBuffer; // Alpha = specular.
layout (binding = 11) uniform sampler2D normalBuffer;   // Alpha reserved for handedness.
layout (binding = 12) uniform sampler2D albedoBuffer;   // Alpha = shininess.

struct PointLight
{
    vec3 position;
    
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
        
    float linear;
    float quadratic;
};

layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec4 brightColor;

in vec2 texCoords;

layout (std140, binding = 0) uniform Matrices
{
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 dirLightSpaceMatrix;
	mat4 spotLightSpaceMatrix;
    mat4 pointShadowMatrices[6];
};

uniform PointLight pointLight;

uniform bool blinn;

// Shadow maps.
layout (binding = 16) uniform samplerCube pointDepthMap;

// Point light shadow mapping.
uniform float pointFar;

uniform vec3 cameraPos;

uniform vec2 screenSize;

float CalcPointShadow(vec3 nrm, vec3 fragPos, vec3 lightPos, samplerCube depthMap)
{
    vec3 lightToFrag = fragPos - lightPos;
    float shadowMapDepth = texture(depthMap, lightToFrag).r * pointFar;
    float bias = max(.05f * (1.f - dot(nrm, normalize(lightToFrag))), .005f);
    float dist = length(lightToFrag);
    // return dist - bias > shadowMapDepth ? 1.f : 0.f;
    
    vec3 sampleOffsetDirections[20] =
    {
        vec3( 1.f,  1.f,  1.f), vec3( 1.f, -1.f,  1.f), vec3(-1.f, -1.f,  1.f), vec3(-1.f,  1.f,  1.f),
        vec3( 1.f,  1.f, -1.f), vec3( 1.f, -1.f, -1.f), vec3(-1.f, -1.f, -1.f), vec3(-1.f,  1.f, -1.f), 
        vec3( 1.f,  1.f,  0.f), vec3( 1.f, -1.f,  0.f), vec3(-1.f, -1.f,  0.f), vec3(-1.f,  1.f,  0.f),
        vec3( 1.f,  0.f,  1.f), vec3(-1.f,  0.f,  1.f), vec3( 1.f,  0.f, -1.f), vec3(-1.f,  0.f, -1.f),
        vec3( 0.f,  1.f,  1.f), vec3( 0.f, -1.f,  1.f), vec3( 0.f, -1.f, -1.f), vec3( 0.f,  1.f, -1.f),
    };
    
    float shadow = 0.f;
    float viewDistance = distance(cameraPos, fragPos);
    float cubeSize = (1.f + (viewDistance / pointFar)) / 25.f;
    for (int i = 0; i < 20; i++)
    {
        float pcfDepth = texture(depthMap, lightToFrag + sampleOffsetDirections[i] * cubeSize).r * pointFar;
        shadow += dist - bias > pcfDepth ? 1.f : 0.f;
    }
    return shadow / 20.f;
}

void main()
{
    vec2 sampleTexCoords = gl_FragCoord.xy / screenSize;
    
    vec3 fragPos = texture(positionBuffer, sampleTexCoords).rgb;
    vec3 cameraDir = normalize(cameraPos - fragPos);
    
    vec3 normal = texture(normalBuffer, sampleTexCoords).rgb;
    vec3 result = vec3(0.f);
    
    vec3 lightPos = pointLight.position;

    float d = distance(fragPos, lightPos);
    float intensity = 1.f / (1.f + pointLight.linear * d + pointLight.quadratic * d * d);

    // Ambient contribution.
    vec3 ambient = texture(albedoBuffer, sampleTexCoords).rgb * pointLight.ambient;

    // Diffuse contribution.
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(normal, lightDir), 0.f);
    vec3 diffuse = texture(albedoBuffer, sampleTexCoords).rgb * diff * pointLight.diffuse;

    // Specular contribution.
    vec3 halfway = normalize(lightDir + cameraDir);
    vec3 reflectionDir = reflect(-lightDir, normal);
    vec3 specVec1 = blinn ? halfway : reflectionDir;
    vec3 specVec2 = blinn ? normal : cameraDir;
    float shininess = texture(albedoBuffer, sampleTexCoords).a;
    float spec = pow(max(dot(specVec1, specVec2), 0.f), shininess);
    vec3 specular = texture(positionBuffer, sampleTexCoords).a * spec * pointLight.specular;

    float shadow = CalcPointShadow(normal, fragPos, pointLight.position, pointDepthMap);
    result += intensity * (1.f - shadow) * (diffuse + specular);
    
    fragColor = vec4(result, 1.f);
    
    float brightness = dot(fragColor.rgb, vec3(.2126f, .7152f, .0722f));
    if (brightness > 1.f)
    {
        brightColor = fragColor;
    }
    else
    {
        brightColor = vec4(vec3(0.f), 1.f);
    }
}
