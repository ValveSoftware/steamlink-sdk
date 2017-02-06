#version 150 core

in vec4 vertexPosition;
in vec2 vertexTexCoord;

out vec2 texCoord;

uniform mat4 modelMatrix;

void main()
{
    texCoord = vertexTexCoord;
    gl_Position = modelMatrix * vertexPosition;
}
