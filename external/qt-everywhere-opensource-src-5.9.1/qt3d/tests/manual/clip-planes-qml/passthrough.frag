#version 150 core

out vec4 fragColor;

in VertexData {
    vec3 position;
    vec3 normal;
} f_in;

void main()
{
    fragColor = vec4(1.0, 1.0, 1.0, 1.0) * vec4(f_in.position, 1.0);
}
