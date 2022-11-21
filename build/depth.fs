#version 330 core

out vec4 fragColor;

float near = .1f;
float far = 100.f;

float LinearizeDepth(float depth)
{
    float z = depth * 2.f - 1.f; // Back to NDC.
    return (2.f * near * far) / (far + near - z * (far - near));
}

void main()
{
    float depth = LinearizeDepth(gl_FragCoord.z) / far; // Divide by far for demonstration.
    fragColor = vec4(vec3(depth), 1.f);
}