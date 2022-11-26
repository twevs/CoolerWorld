#version 330 core

layout (points) in;
layout (triangle_strip, max_vertices = 5) out;

in VS_OUT
{
	vec3 vertColor;
} gs_in[];

out vec3 color;

void main()
{
	color = gs_in[0].vertColor;
	
	gl_Position = gl_in[0].gl_Position + vec4(-.1f, -.1f, 0.f, 0.f);
	EmitVertex();
	
	gl_Position = gl_in[0].gl_Position + vec4(.1f, -.1f, 0.f, 0.f);
	EmitVertex();
	
	gl_Position = gl_in[0].gl_Position + vec4(-.1f, .1f, 0.f, 0.f);
	EmitVertex();
	
	gl_Position = gl_in[0].gl_Position + vec4(.1f, .1f, 0.f, 0.f);
	EmitVertex();
	
	color = vec3(1.f);
	
	gl_Position = gl_in[0].gl_Position + vec4(0.f, .2f, 0.f, 0.f);
	EmitVertex();
	
	EndPrimitive();
}
