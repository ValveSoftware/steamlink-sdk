#version 150 core

in vec3 vertexPosition;
in vec3 vertexNormal;

out vec3 worldPosition;
out vec3 worldNormal;

uniform mat4 modelMatrix;
uniform mat3 modelNormalMatrix;
uniform mat4 modelViewProjection;

void main()
{
    worldNormal = normalize( modelNormalMatrix * vertexNormal );
    worldPosition = vec3( modelMatrix * vec4( vertexPosition, 1.0 ) );

    gl_Position = modelViewProjection * vec4( vertexPosition, 1.0 );
}
