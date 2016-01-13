#version 150 core

in vec4 vCoord;

uniform mat4 matrix;

void main()
{
    gl_Position = matrix * vCoord;
}