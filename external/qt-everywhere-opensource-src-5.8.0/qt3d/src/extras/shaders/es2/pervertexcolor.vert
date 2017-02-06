attribute vec3 vertexPosition;
attribute vec3 vertexNormal;
attribute vec3 vertexColor;

varying vec3 worldPosition;
varying vec3 worldNormal;
varying vec3 color;

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
