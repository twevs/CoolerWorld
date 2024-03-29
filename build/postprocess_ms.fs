#version 330 core

in vec2 texCoords;

out vec4 fragColor;

uniform sampler2DMS tex;

const float offset = 1.f / 300.f;

#define NUM_SAMPLES 4

void main()
{
	vec2 offsets3[9] =
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
	
	vec2 offsets5[25] =
	{
		vec2(-offset * 2.f, offset * 2.f),
		vec2(-offset      , offset * 2.f),
		vec2(0.f          , offset * 2.f),
		vec2(offset       , offset * 2.f),
		vec2(offset * 2.f , offset * 2.f),
	
		vec2(-offset * 2.f, offset),
		vec2(-offset      , offset),
		vec2(0.f          , offset),
		vec2(offset       , offset),
		vec2(offset * 2.f , offset),
	
		vec2(-offset * 2.f, 0.f),
		vec2(-offset      , 0.f),
		vec2(0.f          , 0.f),     
		vec2(offset       , 0.f),    
		vec2(offset * 2.f , 0.f),
		
		vec2(-offset * 2.f, -offset),
		vec2(-offset      , -offset),
		vec2(0.f          , -offset), 
		vec2(offset       , -offset),
		vec2(offset * 2.f , -offset),
		
		vec2(-offset * 2.f, -offset * 2.f),
		vec2(-offset      , -offset * 2.f),
		vec2(0.f          , -offset * 2.f),
		vec2(offset       , -offset * 2.f),
		vec2(offset * 2.f , -offset * 2.f),
	};
	
	float sharpenKernel[9] =
	{
		-1, -1, -1,
		-1,  9, -1,
		-1, -1, -1
	};
	
	float sharpenKernel2[9] =
	{
		0.f,  -1.f, 0.f,
		-1.f,  5.f, -1.f,
		0.f,  -1.f, 0.f
	};
	
	float gaussian3Kernel[9] =
	{
		1.f / 16.f, 2.f / 16.f, 1.f / 16.f,
		2.f / 16.f, 4.f / 16.f, 2.f / 16.f,
		1.f / 16.f, 2.f / 16.f, 1.f / 16.f
	};
	
	float gaussian5Kernel[25] =
	{
		1.f / 256.f, 4.f / 256.f,  6.f / 256.f,  4.f / 256.f,  1.f / 256.f,
		4.f / 256.f, 16.f / 256.f, 24.f / 256.f, 16.f / 256.f, 4.f / 256.f,
		6.f / 256.f, 24.f / 256.f, 36.f / 256.f, 24.f / 256.f, 6.f / 256.f,
		4.f / 256.f, 16.f / 256.f, 24.f / 256.f, 16.f / 256.f, 4.f / 256.f,
		1.f / 256.f, 4.f / 256.f,  6.f / 256.f,  4.f / 256.f,  1.f / 256.f
	};
	
	float unsharpMasking5Kernel[25] =
	{
		1.f / -256.f, 4.f / -256.f,  6.f / -256.f,    4.f / -256.f,  1.f / -256.f,
		4.f / -256.f, 16.f / -256.f, 24.f / -256.f,   16.f / -256.f, 4.f / -256.f,
		6.f / -256.f, 24.f / -256.f, -476.f / -256.f, 24.f / -256.f, 6.f / -256.f,
		4.f / -256.f, 16.f / -256.f, 24.f / -256.f,   16.f / -256.f, 4.f / -256.f,
		1.f / -256.f, 4.f / -256.f,  6.f / -256.f,    4.f / -256.f,  1.f / -256.f
	};
	
	float edgeKernel[9] =
	{
		1.f,  1.f, 1.f,
		1.f, -8.f, 1.f,
		1.f,  1.f, 1.f
	};
	
	float ridgeKernel[9] =
	{
		-1.f,  -1.f, -1.f,
		-1.f,   4.f, -1.f,
		-1.f,  -1.f, -1.f
	};
	
	float boxBlurFactor = 1.f / 9.f;
	float boxBlurKernel[9] =
	{
		boxBlurFactor,  boxBlurFactor, boxBlurFactor,
		boxBlurFactor, -boxBlurFactor, boxBlurFactor,
		boxBlurFactor,  boxBlurFactor, boxBlurFactor
	};
	
	/*
	vec3 sampleTex[9];
	for (int i = 0; i < 9; i++)
	{
		for (int j = 0; j < NUM_SAMPLES; j++)
		{
			vec2 coords = texCoords.st + offsets3[i];
			ivec2 texSize = textureSize(tex);
			ivec2 iCoords = ivec2(texSize * coords);
			sampleTex[i] += texelFetch(tex, iCoords, j);
		}
		sampleTex[i] /= float(NUM_SAMPLES);
	}
	*/
	
	vec4 sampleTex[25];
	for (int i = 0; i < 25; i++)
	{
		for (int j = 0; j < NUM_SAMPLES; j++)
		{
			vec2 coords = texCoords.st + offsets5[i];
			ivec2 texSize = textureSize(tex);
			ivec2 iCoords = ivec2(texSize * coords);
			sampleTex[i] += texelFetch(tex, iCoords, j);
		}
		sampleTex[i] /= float(NUM_SAMPLES);
	}
	
	vec4 color = vec4(0.f);
	for (int i = 0; i < 25; i++)
	{
		color += (sampleTex[i] * unsharpMasking5Kernel[i]);
	}
	
	// fragColor = color;
	for (int j = 0; j < NUM_SAMPLES; j++)
	{
		ivec2 texSize = textureSize(tex);
		ivec2 iCoords = ivec2(texSize * texCoords);
		fragColor += texelFetch(tex, iCoords, j);
	}
	fragColor /= float(NUM_SAMPLES);
}
