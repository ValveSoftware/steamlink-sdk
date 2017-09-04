#version 150 core

in vec3 vertexPosition;
in vec3 vertexNormal;
in vec3 vertexPositionTarget;
in vec3 vertexNormalTarget;

out vec3 worldPosition;
out vec3 worldNormal;

uniform mat4 modelMatrix;
uniform mat3 modelNormalMatrix;
uniform mat4 modelViewProjection;
uniform float interpolator;

void main()
{
    vec3 morphPos;
    vec3 morphNormal;
    if (interpolator > 0.0) {
        // normalized
        morphPos = mix(vertexPosition, vertexPositionTarget, interpolator);
        morphNormal = normalize(mix(vertexNormal, vertexNormalTarget, interpolator));
    } else {
        // relative
        morphPos = vertexPosition + vertexPositionTarget * abs(interpolator);
        morphNormal = normalize(vertexNormal + vertexNormalTarget * abs(interpolator));
    }

    worldNormal = normalize( modelNormalMatrix * morphPos );
    worldPosition = vec3( modelMatrix * vec4( morphPos, 1.0 ) );

    gl_Position = modelViewProjection * vec4( morphPos, 1.0 );
}
