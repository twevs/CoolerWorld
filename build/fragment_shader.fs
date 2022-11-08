#version 330 core

in vec2 ourTexCoord;

out vec4 fragColor;

uniform float mixAlpha;
uniform sampler2D texture1;
uniform sampler2D texture2;

void main()
{
    fragColor = mix(texture(texture1, ourTexCoord), texture(texture2, vec2(-ourTexCoord.x, ourTexCoord.y)), mixAlpha);
}