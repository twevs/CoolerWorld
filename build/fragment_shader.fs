#version 330 core

// in vec2 ourTexCoord;

in vec3 color;

out vec4 fragColor;

void main()
{
    fragColor = vec4(color, 1.f);
}