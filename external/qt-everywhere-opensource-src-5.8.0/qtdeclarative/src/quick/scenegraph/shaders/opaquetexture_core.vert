#version 150 core

uniform mat4 qt_Matrix;

in vec4 qt_VertexPosition;
in vec2 qt_VertexTexCoord;

out vec2 qt_TexCoord;

void main()
{
    qt_TexCoord = qt_VertexTexCoord;
    gl_Position = qt_Matrix * qt_VertexPosition;
}