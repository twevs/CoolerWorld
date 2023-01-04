#version 450 core

in vec2 texCoords;

out vec4 fragColor;

layout (binding = 10) uniform sampler2D tex;

void main()
{
	fragColor = texture(tex, texCoords);
}
