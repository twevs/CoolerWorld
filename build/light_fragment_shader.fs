#version 330 core

// in vec2 ourTexCoord;

out vec4 fragColor;

// uniform float mixAlpha;
// uniform sampler2D texture1;
// uniform sampler2D texture2;
uniform vec3 lightColor;

void main()
{
    // fragColor = mix(texture(texture1, ourTexCoord), texture(texture2, vec2(-ourTexCoord.x, ourTexCoord.y)), mixAlpha);
    fragColor = vec4(lightColor, 1.f);
}