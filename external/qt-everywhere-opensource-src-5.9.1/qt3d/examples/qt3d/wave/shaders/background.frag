#version 150 core

in vec2 texCoord;

out vec4 fragColor;

uniform vec3 color1;
uniform vec3 color2;

void main()
{
    fragColor = vec4( mix( color1, color2, texCoord.t ), 1.0 );
}
