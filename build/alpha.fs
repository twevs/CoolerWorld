#version 330 core
	
in vec2 texCoords;
	
out vec4 fragColor;

uniform sampler2D tex;

void main()
{
	vec4 frag = texture(tex, texCoords);
	if (frag.a < .1f)
	{
		discard;
	}
	else
	{
		fragColor = frag;
	}
}
