#version 150 core

in vec3 vertexPosition;

out VertexData {
    vec3 position;
} v_out;

uniform mat4 mvp;

void main()
{
    v_out.position = vertexPosition;
    gl_Position = mvp * vec4( vertexPosition, 1.0 );
}
