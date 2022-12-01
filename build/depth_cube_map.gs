#version 420 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

layout (std140, binding = 0) uniform Matrices
{
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 dirLightSpaceMatrix;
	mat4 spotLightSpaceMatrix;
    mat4 pointShadowMatrices[6];
};

out vec4 fragPos;

void main()
{
    for (int face = 0; face < 6; face++)
    {
        gl_Layer = face;
        for (int i = 0; i < 3; i++)
        {
            fragPos = gl_in[i].gl_Position;
            gl_Position = pointShadowMatrices[face] * fragPos;
            EmitVertex();
        }
        EndPrimitive();
    }
}
