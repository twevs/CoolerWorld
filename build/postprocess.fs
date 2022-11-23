#version 330 core

in vec2 texCoords;

out vec4 fragColor;

uniform sampler2D tex;

const float offset = 1.f / 300.f;

void main()
{
	vec2 offsets[9] =
	{
		vec2(-offset, offset),  // Top left.
		vec2(0.f    , offset),  // Top centre.
		vec2(offset , offset),  // Top right.
		vec2(-offset, 0.f),     // Centre left.
		vec2(0.f    , 0.f),     // Centre centre.
		vec2(offset , 0.f),     // Centre right.
		vec2(-offset, -offset), // Bottom left.
		vec2(0.f    , -offset), // Bottom centre.
		vec2(offset , -offset), // Bottom right.
	};
	
	float sharpenKernel[9] =
	{
		-1, -1, -1,
		-1,  9, -1,
		-1, -1, -1
	};
	
	float blurKernel[9] =
	{
		1.f / 16.f, 2.f / 16.f, 1.f / 16.f,
		2.f / 16.f, 4.f / 16.f, 2.f / 16.f,
		1.f / 16.f, 2.f / 16.f, 1.f / 16.f
	};
	
	float edgeKernel[9] =
	{
		1.f,  1.f, 1.f,
		1.f, -8.f, 1.f,
		1.f,  1.f, 1.f
	};
	
	vec3 sampleTex[9];
	for (int i = 0; i < 9; i++)
	{
		sampleTex[i] = vec3(texture(tex, texCoords.st + offsets[i]));
	}
	
	vec3 color = vec3(0.f);
	for (int i = 0; i < 9; i++)
	{
		color += (sampleTex[i] * edgeKernel[i]);
	}
	
	// fragColor = vec4(color, 1.f);
	fragColor = texture(tex, texCoords);
}
