#version 330 core

layout (location = 0) in vec3 aPos;

out vec3 texCoords;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;

void main()
{
	texCoords = aPos;
	vec4 pos = projectionMatrix * viewMatrix * vec4(aPos, 1.f);
	gl_Position = pos.xyww;
}
