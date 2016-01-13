#version 150 core

in vec2 sampleCoord;
in vec2 shiftedSampleCoord;

out vec4 color;

uniform sampler2D _qt_texture;
uniform vec4 color;
uniform vec4 styleColor;

void main()
{
    float glyph = texture(_qt_texture, sampleCoord).a;
    float style = clamp(texture(_qt_texture, shiftedSampleCoord).r - glyph,
                        0.0, 1.0);
    fragColor = style * styleColor + glyph * color;
}