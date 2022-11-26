#version 330 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;
    
in VS_OUT
{
	vec3 vs_FragWorldPos;
	vec3 vs_Normal;
	vec2 vs_TexCoords;
} gs_in[];

out vec3 fragWorldPos;
out vec3 normal;
out vec2 texCoords;

uniform float time;
uniform float magnitude;

vec4 ExplodePosition(vec4 position, vec3 direction);

void main()
{
    vec3 yVec = vec3(gl_in[0].gl_Position - gl_in[1].gl_Position);
    vec3 xVec = vec3(gl_in[2].gl_Position - gl_in[1].gl_Position);
    vec3 localNormal = normalize(cross(yVec, xVec));
    
    fragWorldPos = gs_in[0].vs_FragWorldPos;
    normal = gs_in[0].vs_Normal;
    texCoords = gs_in[0].vs_TexCoords;
    gl_Position = ExplodePosition(gl_in[0].gl_Position, localNormal);
    EmitVertex();
    
    fragWorldPos = gs_in[1].vs_FragWorldPos;
    normal = gs_in[1].vs_Normal;
    texCoords = gs_in[1].vs_TexCoords;
    gl_Position = ExplodePosition(gl_in[1].gl_Position, localNormal);
    EmitVertex();
    
    fragWorldPos = gs_in[2].vs_FragWorldPos;
    normal = gs_in[2].vs_Normal;
    texCoords = gs_in[2].vs_TexCoords;
    gl_Position = ExplodePosition(gl_in[2].gl_Position, localNormal);
    EmitVertex();
    
    EndPrimitive();
}

vec4 ExplodePosition(vec4 position, vec3 direction)
{
    return position + vec4(direction, 0.f) * ((cos(time) + 1.f) / 2.f) * magnitude;
}
