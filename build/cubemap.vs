#version 450 core

layout (location = 0) in vec3 aPos;

out vec3 texCoords;

layout (std140, binding = 0) uniform Matrices
{
	mat4 viewMatrix;
	mat4 projectionMatrix;
};

void main()
{
	texCoords = aPos;
	vec4 pos = projectionMatrix * viewMatrix * vec4(aPos, 1.f);
	gl_Position = pos.xyww;
}
