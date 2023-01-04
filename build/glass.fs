#version 450 core

in vec3 fragWorldPos;
in vec3 normal;
in vec2 texCoords;

out vec4 fragColor;

uniform vec3 cameraPos;
layout (binding = 10) uniform sampler2D tex;
layout (binding = 11) uniform samplerCube skybox;

void main()
{
	vec3 cameraToFragment = normalize(fragWorldPos - cameraPos);
	float refractiveIndex = 1.f / 1.52f;
	vec3 refraction = refract(cameraToFragment, normal, refractiveIndex);
	
	fragColor = texture(tex, texCoords) * .5f + texture(skybox, refraction) * .5f;
}
