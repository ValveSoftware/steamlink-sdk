#version 150 core

in vec2 sampleCoord;

out vec4 fragColor;

uniform sampler2D _qt_texture;
uniform vec4 color;
uniform float alphaMin;
uniform float alphaMax;

void main()
{
    fragColor = color * smoothstep(alphaMin, alphaMax,
                                   texture(_qt_texture, sampleCoord).r);
}