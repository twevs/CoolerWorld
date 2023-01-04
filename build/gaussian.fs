#version 450 core

out vec4 fragColor;

in vec2 texCoords;

layout (binding = 10) uniform sampler2D image;

uniform bool horizontal;

void main()
{
    float weight[5] = {.227027f, .1945946f, .1216216f, .054054f, .016216f};
    
    vec2 texOffset = 1.f / textureSize(image, 0);
    vec3 result = texture(image, texCoords).rgb * weight[0];
    if (horizontal)
    {
        for (int i = 1; i < 5; i++)
        {
            result += texture(image, texCoords + vec2(texOffset.x * i, 0.f)).rgb * weight[i];
            result += texture(image, texCoords - vec2(texOffset.x * i, 0.f)).rgb * weight[i];
        }
    }
    else
    {
        for (int i = 1; i < 5; i++)
        {
            result += texture(image, texCoords + vec2(0.f, texOffset.y * i)).rgb * weight[i];
            result += texture(image, texCoords - vec2(0.f, texOffset.y * i)).rgb * weight[i];
        }
    }
    fragColor = vec4(result, 1.f);
}
