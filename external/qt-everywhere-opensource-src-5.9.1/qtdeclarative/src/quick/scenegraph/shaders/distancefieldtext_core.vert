#version 150 core

in vec4 vCoord;
in vec2 tCoord;

out vec2 sampleCoord;

uniform mat4 matrix;
uniform vec2 textureScale;

void main()
{
     sampleCoord = tCoord * textureScale;
     gl_Position = matrix * vCoord;
}