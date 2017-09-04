#version 150 core

in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;

out vec3 worldPosition;
out vec3 worldNormal;
out vec2 texCoord;

uniform mat4 modelMatrix;
uniform mat3 modelNormalMatrix;
uniform mat4 mvp;

uniform vec2 texCoordScale;
uniform vec2 texCoordBias;

void main()
{
    texCoord = vertexTexCoord * texCoordScale + texCoordBias;
    worldNormal = normalize(modelNormalMatrix * vertexNormal);
    worldPosition = vec3(modelMatrix * vec4(vertexPosition, 1.0));

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
