#version 150 core

in vec3 vertexPosition;
in vec3 vertexNormal;
in vec3 vertexColor;

out vec3 worldPosition;
out vec3 worldNormal;
out vec3 color;

uniform mat4 modelMatrix;
uniform mat3 modelNormalMatrix;
uniform mat4 mvp;

void main()
{
    worldNormal = normalize( modelNormalMatrix * vertexNormal );
    worldPosition = vec3( modelMatrix * vec4( vertexPosition, 1.0 ) );
    color = vertexColor;

    gl_Position = mvp * vec4( vertexPosition, 1.0 );
}
