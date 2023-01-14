#version 450 core
	
layout (binding = 10) uniform sampler2D positionBuffer;
layout (binding = 11) uniform sampler2D normalBuffer;
layout (binding = 12) uniform sampler2D noiseTexture;

#define SSAO_KERNEL_SIZE 64

uniform vec3 samples[SSAO_KERNEL_SIZE];

uniform mat4 cameraViewMatrix;
uniform mat4 cameraProjectionMatrix;

uniform vec2 screenSize;
uniform float radius = .5f;
uniform float power = 1.f;

in vec2 texCoords;

out float fragColor;

void main()
{
	vec2 noiseScale = vec2(screenSize.x / 4.f, screenSize.y / 4.f);
	vec3 fragPosWS = texture(positionBuffer, texCoords).rgb;
	if (all(equal(vec3(0.f), fragPosWS)))
	{
		discard;
	}
	
	vec3 fragPosVS = (cameraViewMatrix * vec4(fragPosWS, 1.f)).xyz;
	vec3 normalVS = (cameraViewMatrix * vec4(texture(normalBuffer, texCoords).rgb, 0.f)).xyz;
	vec3 randomVec = texture(noiseTexture, texCoords * noiseScale).xyz;
	
	vec3 tangent = normalize(randomVec - normalVS * dot(randomVec, normalVS));
	vec3 bitangent = cross(normalVS, tangent);
	mat3 tbn = mat3(tangent, bitangent, normalVS);
	
	float occlusion = 0.f;
	int rejected = 0;
	for (int i = 0; i < SSAO_KERNEL_SIZE; i++)
	{
		vec3 samplePosVS = tbn * samples[i];
		samplePosVS = fragPosVS + samplePosVS * radius;
		
		vec4 samplePosScreen = cameraProjectionMatrix * vec4(samplePosVS, 1.f);
		samplePosScreen.xy /= samplePosScreen.w;
		samplePosScreen.xy = samplePosScreen.xy * .5f + .5f;
		
		vec3 renderedPosScreen = texture(positionBuffer, samplePosScreen.xy).rgb;
		if (all(equal(vec3(0.f), renderedPosScreen)))
		{
			rejected++;
			continue;
		}
		float sampleDepth = (cameraViewMatrix * vec4(renderedPosScreen, 1.f)).z;
		float bias = .025f;
		float rangeCheck = smoothstep(0.f, 1.f, radius / abs(fragPosVS.z - sampleDepth));
		occlusion += ((sampleDepth >= samplePosVS.z + bias) ? 1.f : 0.f) * rangeCheck;
	}
	occlusion = 1.f - (occlusion / float(SSAO_KERNEL_SIZE - rejected));
	fragColor = pow(occlusion, power);
}
