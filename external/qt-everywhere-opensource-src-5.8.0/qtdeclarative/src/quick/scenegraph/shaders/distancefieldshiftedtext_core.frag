#version 150 core

in vec2 sampleCoord;
in vec2 shiftedSampleCoord;

out vec4 fragColor;

uniform sampler2D _qt_texture;
uniform vec4 color;
uniform vec4 styleColor;
uniform float alphaMin;
uniform float alphaMax;

void main()
{
    float a = smoothstep(alphaMin, alphaMax, texture(_qt_texture, sampleCoord).r);
    vec4 shifted = styleColor * smoothstep(alphaMin, alphaMax,
                                           texture(_qt_texture, shiftedSampleCoord).r);
    fragColor = mix(shifted, color, a);
}