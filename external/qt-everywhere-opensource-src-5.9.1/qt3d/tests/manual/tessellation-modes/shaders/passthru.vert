#version 400 core

in vec3 vertexPosition;

void main()
{
    // We do the transformations later in the
    // tessellation evaluation shader
    gl_Position = vec4( vertexPosition, 1.0 );
}
