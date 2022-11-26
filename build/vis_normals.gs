#version 420 core

layout (triangles) in;
layout (line_strip, max_vertices = 6) out;
    
in VS_OUT
{
	vec3 vs_Normal;
} gs_in[];

out vec3 normal;

layout (std140, binding = 0) uniform Matrices
{
	mat4 viewMatrix;
	mat4 projectionMatrix;
};

void GenerateLine(int index)
{
    gl_Position = projectionMatrix * gl_in[index].gl_Position;    
    EmitVertex();
    gl_Position = projectionMatrix *
        (gl_in[index].gl_Position + vec4(gs_in[index].vs_Normal, 0.f) * .2f);
    EmitVertex();
    EndPrimitive();
}

void main()
{
    GenerateLine(0);
    GenerateLine(1);
    GenerateLine(2);
}
