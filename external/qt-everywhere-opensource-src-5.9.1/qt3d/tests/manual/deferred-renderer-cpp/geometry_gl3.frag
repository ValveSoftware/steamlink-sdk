#version 140

in vec4 color0;
in vec3 position0;
in vec3 normal0;

out vec4 color;
out vec3 position;
out vec3 normal;

void main()
{
    color = color0;
    position = position0;
    normal = normal0;
}
