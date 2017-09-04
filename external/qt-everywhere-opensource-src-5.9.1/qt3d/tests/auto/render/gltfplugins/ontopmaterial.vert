#version 150 core

in vec3 vertexPosition;
in vec3 vertexOffset;

uniform mat4 modelViewProjection;
uniform vec3 globalOffset;
uniform int extraYOffset;
uniform bool reverseOffset;

void main()
{
    vec4 offset = vec4(globalOffset, 0.0) + vec4(0.0f, float(extraYOffset), 0.0f, 0.0f);
    if (reverseOffset)
        offset *= -1.0f;
    gl_Position = modelViewProjection
            * (vec4(vertexPosition, 1.0) + offset + vec4(vertexOffset, 0.0));
    // Set the Z value of the vertex so that it'll always get drawn on top of everything else
    gl_Position.z = -1.0;
}
