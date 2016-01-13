#version 150 core

in vec4 vCoord;
in vec2 tCoord;

out vec2 sampleCoord;
out vec2 sCoordUp;
out vec2 sCoordDown;
out vec2 sCoordLeft;
out vec2 sCoordRight;

uniform mat4 matrix;
uniform vec2 textureScale;
uniform vec2 shift;

void main()
{
     sampleCoord = tCoord * textureScale;
     sCoordUp = (tCoord - vec2(0.0, -1.0)) * textureScale;
     sCoordDown = (tCoord - vec2(0.0, 1.0)) * textureScale;
     sCoordLeft = (tCoord - vec2(-1.0, 0.0)) * textureScale;
     sCoordRight = (tCoord - vec2(1.0, 0.0)) * textureScale;
     gl_Position = matrix * vCoord;
}