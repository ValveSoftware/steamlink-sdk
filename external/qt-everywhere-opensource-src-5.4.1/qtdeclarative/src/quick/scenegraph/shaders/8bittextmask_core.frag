#version 150 core

in vec2 sampleCoord;

out vec4 fragColor;

uniform sampler2D _qt_texture;
uniform vec4 color;

void main()
{
    fragColor = color * texture(_qt_texture, sampleCoord).r;
}