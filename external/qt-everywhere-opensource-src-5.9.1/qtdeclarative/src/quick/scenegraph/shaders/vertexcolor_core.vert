#version 150 core

in vec4 vertexCoord;
in vec4 vertexColor;

out vec4 color;

uniform mat4 matrix;
uniform float opacity;

void main()
{
    gl_Position = matrix * vertexCoord;
    color = vertexColor * opacity;
}