#version 110

varying vec4 color0;
varying vec3 position0;
varying vec3 normal0;

void main()
{
    gl_FragData[0] = color0;
    gl_FragData[1] = vec4(position0, 0);
    gl_FragData[2] = vec4(normal0, 0);
}
