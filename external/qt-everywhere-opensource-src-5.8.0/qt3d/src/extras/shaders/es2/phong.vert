attribute vec3 vertexPosition;
attribute vec3 vertexNormal;

varying vec3 worldPosition;
varying vec3 worldNormal;

uniform mat4 modelMatrix;
uniform mat3 modelNormalMatrix;
uniform mat4 modelViewProjection;

void main()
{
    worldNormal = normalize( modelNormalMatrix * vertexNormal );
    worldPosition = vec3( modelMatrix * vec4( vertexPosition, 1.0 ) );

    gl_Position = modelViewProjection * vec4( vertexPosition, 1.0 );
}
