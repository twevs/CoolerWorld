#version 450 core

in vec3 texCoords;

out vec4 fragColor;

layout (binding = 10) uniform samplerCube skybox;

void main()
{
	fragColor = texture(skybox, texCoords);
}
