#version 150 core

in vec4 av;
in vec2 at;

out vec2 t;

void main()
{
    gl_Position = av;
    t = at;
}