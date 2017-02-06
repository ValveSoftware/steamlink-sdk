#version 110

attribute vec4 vertexPosition;
attribute vec3 vertexNormal;

varying vec4 color0;
varying vec3 position0;
varying vec3 normal0;

uniform mat4 mvp;
uniform mat4 modelView;
uniform mat3 modelViewNormal;
uniform vec4 meshColor;

void main()
{
    color0 = meshColor;
    position0 = vec3(modelView * vertexPosition);
    normal0 = normalize(modelViewNormal * vertexNormal);
    gl_Position = mvp * vertexPosition;
}
