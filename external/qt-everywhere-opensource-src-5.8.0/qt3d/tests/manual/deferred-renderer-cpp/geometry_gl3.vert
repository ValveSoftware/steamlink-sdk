#version 140

in vec4 vertexPosition;
in vec3 vertexNormal;

out vec4 color0;
out vec3 position0;
out vec3 normal0;

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
