#version 450 core
	
out float fragColor;

in vec2 texCoords;

layout (binding = 10) uniform sampler2D ssaoInput;

void main()
{
	vec2 texelSize = 1.f / vec2(textureSize(ssaoInput, 0));
	float result = 0.f;
	for (int x = -2; x < 2; x++)
	{
		for (int y = -2; y < 2; y++)
		{
			vec2 offset = vec2(float(x), float(y)) * texelSize;
			result += texture(ssaoInput, texCoords + offset).r;
		}
	}
	fragColor = result / 16.f;
}
