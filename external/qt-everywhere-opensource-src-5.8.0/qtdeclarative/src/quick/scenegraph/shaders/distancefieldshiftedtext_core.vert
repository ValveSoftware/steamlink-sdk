#version 150 core

in vec4 vCoord;
in vec2 tCoord;

out vec2 sampleCoord;
out vec2 shiftedSampleCoord;

uniform mat4 matrix;
uniform vec2 textureScale;
uniform vec2 shift;

void main()
{
     sampleCoord = tCoord * textureScale;
     shiftedSampleCoord = (tCoord - shift) * textureScale;
     gl_Position = matrix * vCoord;
}