#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in mat4 aModelMatrix;
layout (location = 7) in float aRadius;
layout (location = 8) in float aY;

out vec3 fragWorldPos;
out vec3 normal;
out vec2 texCoords;

layout (std140, binding = 0) uniform Matrices
{
	mat4 viewMatrix;
	mat4 projectionMatrix;
};
uniform float time;

#define PI 3.1415926536f

void main()
{
    float angle = (float(gl_InstanceID) * (time / 60.f) / 100000.f) * 2 * PI;
	float radius = aRadius;
    float x = cos(angle) * radius;
	float y = aY;
    float z = sin(angle) * radius;
    mat4 modelMatrix = aModelMatrix;
	modelMatrix[3][0] = x;
	modelMatrix[3][1] = y;
	modelMatrix[3][2] = z;
	
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.f);
    fragWorldPos = vec3(modelMatrix * vec4(aPos, 1.f));
	mat3 normalMatrix = mat3(transpose(inverse(modelMatrix)));
    normal = normalize(normalMatrix * aNormal);
    texCoords = aTexCoords;
}