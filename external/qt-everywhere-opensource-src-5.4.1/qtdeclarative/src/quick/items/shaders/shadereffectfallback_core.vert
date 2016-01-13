#version 150 core

in vec4 v;

uniform mat4 qt_Matrix;

void main()
{
    gl_Position = qt_Matrix * v;
}